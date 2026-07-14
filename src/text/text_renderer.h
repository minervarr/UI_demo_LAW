#pragma once
#include <vulkan/vulkan.h>
#include <atomic>
#include <string_view>
#include <vector>
#include "msdf.hh"
#include "msdf_renderer.hh"
#include "asset_reader.hh"
#include "utf8.hh"

// FileByteReader resolves relative paths against the process CWD, which
// breaks the vendored renderer's internal relative loads ("shaders/
// msdf_vert.spv") whenever the app is launched from anywhere other than the
// exe directory (text silently never initializes). Resolve relative paths
// against the exe directory instead; absolute paths pass straight through.
struct ExeRelativeReader : AssetReader {
    bool read(const char* path, std::vector<uint8_t>& out) override;
 private:
    FileByteReader inner_;
};

// Thin adapter over vulkan_font_engine's MTSDF text stack (MsdfFont +
// MsdfTextRenderer), wiring Regular (weight 0) + Bold (weight 1) + Italic
// (weight 2) rendering into the app. Text color is always emitted as r=g=b
// (see drawText), so the sampled output stays grayscale even though the
// atlas itself is MTSDF (multi-channel internally).
//
// Bold and Italic are real baked faces (FontStyle::Bold / FontStyle::Italic,
// resolved via MsdfFont::keyForStyle()+layoutByKey()) — not a synthesized
// thickened/sheared Roman glyph. The two are mutually exclusive: the vendored
// font engine has no FontStyle::BoldItalic (only Roman/Bold/Math/Italic), so
// there is no real combined-face glyph to draw. Passing both bold=true and
// italic=true is unsupported/undefined (bold wins) — see text_renderer.cpp.
class TextRenderer {
 public:
    // Phase 1: CPU-only font rasterization/caching (msdfgen + FreeType via
    // MsdfFont::generate()/addStyle()/saveCache() — no Vulkan calls, no
    // touching of renderer_/device state). This is the slow part (10-20s on
    // a cache miss) and is safe to run on a background std::thread; call it
    // from there rather than on the main thread so the message pump keeps
    // running. Sets fontsBaked_ (release-ordered) when done.
    // extraTexts: additional strings (e.g. non-Latin showcase lines) whose
    // codepoints get baked via ensureCodepointsBaked() as part of this same
    // background-thread phase, so no additional bake stall happens later.
    bool bakeFonts(std::vector<std::string> extraTexts = {});
    // Phase 2: Vulkan GPU resource creation (MsdfTextRenderer::init +
    // createResources for the three weights). Must be called on the main
    // thread, and only after fontsBaked() reports true (acquire-ordered
    // read) — i.e. after bakeFonts() has fully finished on the background
    // thread.
    bool finishGpuInit(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass,
             uint32_t screenWidth, uint32_t screenHeight);
    // True once bakeFonts() has finished (any thread may call this to poll).
    bool fontsBaked() const { return fontsBaked_.load(std::memory_order_acquire); }
    // Valid only once fontsBaked() is true: whether the bake actually
    // succeeded (false on e.g. a missing/corrupt font file). Set on the
    // background thread strictly before the fontsBaked_ release-store, so
    // the same acquire-read that observes fontsBaked()==true also makes this
    // value visible without needing its own atomic.
    bool bakeSucceeded() const { return bakeSucceeded_; }
    // Scans `text`'s UTF-8 codepoints; for any not already covered by the
    // primary New Computer Modern face, tries a fixed ordered fallback font list
    // (CJK/Korean) via MsdfFont::bakeCodepoints(), which appends only the
    // missing glyphs to the shared default-face table (no new atlas/style
    // slot). CPU-only (msdfgen), same threading contract as bakeFonts() —
    // call before fontsBaked() is observed true by the main thread, or
    // accept a bake stall if called later for genuinely new text.
    // Returns how many glyphs were newly baked (0 = everything was already
    // covered) so bakeFonts() can re-save the cache only when needed.
    int ensureCodepointsBaked(std::string_view text);
    // Convenience wrapper: bakeFonts() then finishGpuInit(). Runs Vulkan
    // calls too, so only ever call this from the main thread (kept for
    // callers that don't need the two-phase split, e.g. tests).
    bool init(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass,
             uint32_t screenWidth, uint32_t screenHeight);
    // Must be called once, inside a command buffer that will be submitted
    // before any draw() call — uploads the atlas texture(s) to the GPU.
    void recordAtlasUpload(VkCommandBuffer cmd);
    // Lays out `text` at (x, baselineY) in screen px and queues it for the
    // next draw() call this frame. bold/italic select the Bold/Italic weight
    // slot (mutually exclusive — see class comment; bold wins if both set).
    void drawText(std::string_view text, float x, float baselineY, float sizePx,
                 float gray, bool bold = false, bool italic = false);
    // Mirror of PrimitiveBatch::setClip/clearClip: text queued while a clip
    // is set is scissored to that rect at draw() time, so scrollable page
    // content clips at its container edge in lockstep with the shapes.
    void setClip(float x, float y, float w, float h);
    void clearClip();
    // Uploads all glyph quads queued by drawText() since the last draw()
    // and records the actual draw calls.
    void draw(VkCommandBuffer cmd, VkExtent2D extent);
    float textWidth(std::string_view text, float sizePx) const;
    void cleanup();

    // True once the offline-baked MATH atlas (fonts/math/font.msdf +
    // atlas.rgba) has been loaded and its GPU resources created (weight
    // slot 3). Loading happens inside finishGpuInit() (Step 2 below), so it
    // shares that method's existing fontsBaked()-gated call site in
    // main.cpp -- it doesn't strictly need to wait for the CJK/Bold/Italic
    // background bake (MsdfFont::load() just reads two pre-baked files and
    // uploads to GPU, no msdfgen rasterization), but piggybacking on the
    // existing gate avoids adding a third main.cpp readiness flag, and the
    // added wait is negligible (load() is fast) compared to the bake it's
    // riding along with.
    bool hasMathFont() const { return mathFontLoaded_; }
    const MsdfFont& mathFont() const { return mathFont_; }
    // Queues one math glyph (addressed by MsdfFont::mathKey()/glyphByKey()
    // key, NOT a codepoint) for the next draw() call. Only valid once
    // hasMathFont() is true.
    void drawMathGlyph(uint32_t key, float x, float y, float sizePx, float gray);

 private:
    // A run of queued glyph verts (per weight slot) sharing one scissor
    // state; draw() issues one renderer_.draw per segment with the matching
    // scissor rect. Same lazy-split scheme as PrimitiveBatch's DrawRange.
    struct TextSegment {
        uint32_t firstVert = 0;
        uint32_t vertCount = 0;
        bool hasClip = false;
        float clipX = 0, clipY = 0, clipW = 0, clipH = 0;
    };
    // Extends segments_[weightIdx] to cover verts appended by the caller
    // (vertsAdded glyph-verts at the end of that weight's pending vector),
    // opening a new segment if the clip state changed.
    void noteSegment(int weightIdx, uint32_t vertsAdded);

    ExeRelativeReader assets_;
    MsdfFont font_;
    MsdfTextRenderer renderer_;
    std::vector<float> pendingRegularVerts_;
    std::vector<float> pendingBoldVerts_;
    std::vector<float> pendingItalicVerts_;
    std::atomic<bool> fontsBaked_{false};
    bool bakeSucceeded_ = false;
    MsdfFont mathFont_;
    bool mathFontLoaded_ = false;
    std::vector<float> pendingMathVerts_;
    std::vector<TextSegment> segments_[4];
    bool clipActive_ = false;
    float clipX_ = 0, clipY_ = 0, clipW_ = 0, clipH_ = 0;
};
