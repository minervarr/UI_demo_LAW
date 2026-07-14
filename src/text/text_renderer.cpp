#include "text_renderer.h"
#include "../platform/paths.h"
#include <algorithm>

bool ExeRelativeReader::read(const char* path, std::vector<uint8_t>& out) {
    bool absolute = path && path[0] &&
        (path[1] == ':' || path[0] == '\\' || path[0] == '/');
    if (absolute) return inner_.read(path, out);
    std::string resolved = exeDirectory() + "\\" + path;
    return inner_.read(resolved.c_str(), out);
}

int TextRenderer::ensureCodepointsBaked(std::string_view text) {
    static const char* kFallbackFonts[] = {
        "fandol\\FandolHei-Regular.otf",
        "haranoaji\\HaranoAjiGothic-Regular.otf",
        "unfonts-core\\UnDotum.ttf",
    };
    std::string exeDir = exeDirectory();
    int newlyBaked = 0;
    size_t i = 0;
    while (i < text.size()) {
        uint32_t cp = utf8::nextCodepoint(text, i);
        if (font_.hasCodepoint(cp)) continue;
        for (const char* rel : kFallbackFonts) {
            std::string path = exeDir + "\\fonts\\" + rel;
            int baked = font_.bakeCodepoints(assets_, path.c_str(), {cp});
            if (baked > 0) { newlyBaked += baked; break; }
        }
    }
    return newlyBaked;
}

bool TextRenderer::bakeFonts(std::vector<std::string> extraTexts) {
    // CPU-only: msdfgen/FreeType rasterization + disk cache load/save. No
    // Vulkan calls, no touching of renderer_/device state — safe to run on
    // a background thread (see header comment / callers).
    std::string exeDir = exeDirectory();
    std::string regularPath = exeDir + "\\fonts\\newcomputermodern\\NewCM10-Regular.otf";
    std::string boldPath = exeDir + "\\fonts\\newcomputermodern\\NewCM10-Bold.otf";
    std::string italicPath = exeDir + "\\fonts\\newcomputermodern\\NewCM10-Italic.otf";
    std::string cachePath = exeDir + "\\fonts\\newcm10-regular.msdf.cache";

    bool ok = font_.generate(assets_, regularPath.c_str(), cachePath.c_str());
    if (ok) {
        if (!font_.hasStyle(FontStyle::Bold)) {
            if (font_.addStyle(assets_, boldPath.c_str(), FontStyle::Bold)) {
                font_.saveCache(cachePath.c_str());
            }
        }
        if (!font_.hasStyle(FontStyle::Italic)) {
            if (font_.addStyle(assets_, italicPath.c_str(), FontStyle::Italic)) {
                font_.saveCache(cachePath.c_str());
            }
        }
        // Persist fallback (CJK) glyphs too: without this save the cache
        // never contains them, so every launch re-rasterized the fallback
        // sets from scratch (~10s of the observed startup time). Save only
        // when something new was actually baked so a fully-cached launch
        // doesn't rewrite the ~22MB cache file.
        int newFallbackGlyphs = 0;
        for (const auto& t : extraTexts) newFallbackGlyphs += ensureCodepointsBaked(t);
        if (newFallbackGlyphs > 0) font_.saveCache(cachePath.c_str());
    }
    bakeSucceeded_ = ok;
    fontsBaked_.store(true, std::memory_order_release);
    return ok;
}

bool TextRenderer::finishGpuInit(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass,
                        uint32_t screenWidth, uint32_t screenHeight) {
    // Vulkan GPU resource creation. Main thread only, and only after
    // fontsBaked() is true (caller's responsibility to check first).
    renderer_.init(device, physicalDevice, assets_, screenWidth, screenHeight);
    renderer_.createResources(renderPass, font_, /*weightIdx=*/0); // Regular
    renderer_.createResources(renderPass, font_, /*weightIdx=*/1); // Bold
    renderer_.createResources(renderPass, font_, /*weightIdx=*/2); // Italic

    std::string exeDir = exeDirectory();
    std::string mathMetrics = exeDir + "\\fonts\\math\\font.msdf";
    std::string mathAtlas = exeDir + "\\fonts\\math\\atlas.rgba";
    if (mathFont_.load(assets_, mathMetrics.c_str(), mathAtlas.c_str())) {
        renderer_.createResources(renderPass, mathFont_, /*weightIdx=*/3);
        mathFontLoaded_ = true;
    }
    return true;
}

bool TextRenderer::init(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass,
                        uint32_t screenWidth, uint32_t screenHeight) {
    if (!bakeFonts()) return false;
    return finishGpuInit(device, physicalDevice, renderPass, screenWidth, screenHeight);
}

void TextRenderer::recordAtlasUpload(VkCommandBuffer cmd) {
    renderer_.recordAtlasUpload(cmd, 0);
    renderer_.recordAtlasUpload(cmd, 1);
    renderer_.recordAtlasUpload(cmd, 2);
    if (mathFontLoaded_) renderer_.recordAtlasUpload(cmd, 3);
}

void TextRenderer::setClip(float x, float y, float w, float h) {
    clipActive_ = true;
    clipX_ = x; clipY_ = y; clipW_ = w; clipH_ = h;
}

void TextRenderer::clearClip() {
    clipActive_ = false;
}

void TextRenderer::noteSegment(int weightIdx, uint32_t vertsAdded) {
    if (vertsAdded == 0) return;
    auto& segs = segments_[weightIdx];
    if (!segs.empty()) {
        TextSegment& cur = segs.back();
        bool sameState = cur.hasClip == clipActive_ &&
            (!clipActive_ || (cur.clipX == clipX_ && cur.clipY == clipY_ &&
                              cur.clipW == clipW_ && cur.clipH == clipH_));
        if (sameState) {
            cur.vertCount += vertsAdded;
            return;
        }
    }
    TextSegment s;
    s.firstVert = segs.empty() ? 0 : segs.back().firstVert + segs.back().vertCount;
    s.vertCount = vertsAdded;
    s.hasClip = clipActive_;
    s.clipX = clipX_; s.clipY = clipY_; s.clipW = clipW_; s.clipH = clipH_;
    segs.push_back(s);
}

void TextRenderer::drawText(std::string_view text, float x, float baselineY, float sizePx,
                            float gray, bool bold, bool italic) {
    // Bold/Italic are mutually exclusive real baked faces (no combined
    // FontStyle::BoldItalic in the vendored engine — see text_renderer.h).
    // If both are requested, bold wins rather than silently faking a
    // combination.
    FontStyle style = bold ? FontStyle::Bold : (italic ? FontStyle::Italic : FontStyle::Roman);
    auto& out = bold ? pendingBoldVerts_ : (italic ? pendingItalicVerts_ : pendingRegularVerts_);
    int weightIdx = bold ? 1 : (italic ? 2 : 0);
    size_t floatsBefore = out.size();
    float penX = x;
    if (style == FontStyle::Roman) {
        size_t i = 0;
        while (i < text.size()) {
            uint32_t cp = utf8::nextCodepoint(text, i);
            penX = font_.emitGlyph(out, cp, penX, baselineY, sizePx, gray, gray, gray, 1.0f);
        }
        noteSegment(weightIdx,
                    (uint32_t)((out.size() - floatsBefore) / MsdfFont::FLOATS_PER_VERT));
        return;
    }
    auto vert = [&](float vx, float vy, float u, float v) {
        out.push_back(vx); out.push_back(vy); out.push_back(u); out.push_back(v);
        out.push_back(gray); out.push_back(gray); out.push_back(gray); out.push_back(1.0f);
    };
    size_t i = 0;
    while (i < text.size()) {
        uint32_t cp = utf8::nextCodepoint(text, i);
        uint32_t key = font_.keyForStyle(style, cp);
        if (key == 0) {
            // Not covered by this style's baked face — fall back to the
            // Roman glyph rather than silently dropping the character.
            penX = font_.emitGlyph(out, cp, penX, baselineY, sizePx, gray, gray, gray, 1.0f);
            continue;
        }
        GlyphQuad q;
        penX = font_.layoutByKey(key, penX, baselineY, sizePx, q);
        if (!q.draw) continue;
        vert(q.x0, q.y0, q.u0, q.v0); vert(q.x1, q.y0, q.u1, q.v0); vert(q.x1, q.y1, q.u1, q.v1);
        vert(q.x0, q.y0, q.u0, q.v0); vert(q.x1, q.y1, q.u1, q.v1); vert(q.x0, q.y1, q.u0, q.v1);
    }
    noteSegment(weightIdx,
                (uint32_t)((out.size() - floatsBefore) / MsdfFont::FLOATS_PER_VERT));
}

void TextRenderer::drawMathGlyph(uint32_t key, float x, float y, float sizePx, float gray) {
    if (!mathFontLoaded_) return;
    GlyphQuad q;
    mathFont_.layoutByKey(key, x, y, sizePx, q);
    if (!q.draw) return;
    auto& out = pendingMathVerts_;
    auto vert = [&](float vx, float vy, float u, float v) {
        out.push_back(vx); out.push_back(vy); out.push_back(u); out.push_back(v);
        out.push_back(gray); out.push_back(gray); out.push_back(gray); out.push_back(1.0f);
    };
    vert(q.x0, q.y0, q.u0, q.v0); vert(q.x1, q.y0, q.u1, q.v0); vert(q.x1, q.y1, q.u1, q.v1);
    vert(q.x0, q.y0, q.u0, q.v0); vert(q.x1, q.y1, q.u1, q.v1); vert(q.x0, q.y1, q.u0, q.v1);
    noteSegment(3, 6);
}

void TextRenderer::draw(VkCommandBuffer cmd, VkExtent2D extent) {
    // The vendored renderer converts pixel coords to clip space with its
    // stored screen size; if it lags the swapchain after a resize, glyphs
    // beyond the old extent are NDC-clipped away (observed: all text past
    // x=1280/y=800 vanishing in a maximized window).
    renderer_.setScreenSize(extent.width, extent.height);
    std::vector<float>* pending[4] = {
        &pendingRegularVerts_, &pendingBoldVerts_, &pendingItalicVerts_, &pendingMathVerts_};
    for (int w = 0; w < 4; w++) {
        auto& verts = *pending[w];
        if (verts.empty()) continue;
        uint32_t count = (uint32_t)(verts.size() / MsdfFont::FLOATS_PER_VERT);
        renderer_.uploadGlyphQuads(verts.data(), count, w);
        // One draw per clip segment (renderer_.draw's sx/sy/sw/sh feed
        // vkCmdSetScissor directly). Clip rects are intersected with the
        // framebuffer so a partly off-screen container can't produce an
        // invalid scissor.
        for (const TextSegment& seg : segments_[w]) {
            int32_t sx = 0, sy = 0;
            uint32_t sw = extent.width, sh = extent.height;
            if (seg.hasClip) {
                int32_t x0 = std::max(0, (int32_t)seg.clipX);
                int32_t y0 = std::max(0, (int32_t)seg.clipY);
                int32_t x1 = std::min((int32_t)extent.width, (int32_t)(seg.clipX + seg.clipW + 0.5f));
                int32_t y1 = std::min((int32_t)extent.height, (int32_t)(seg.clipY + seg.clipH + 0.5f));
                if (x1 <= x0 || y1 <= y0) continue;
                sx = x0; sy = y0; sw = (uint32_t)(x1 - x0); sh = (uint32_t)(y1 - y0);
            }
            renderer_.draw(cmd, renderer_.vertOffset(w) + seg.firstVert, seg.vertCount,
                           0, 0, sx, sy, sw, sh, w);
        }
        verts.clear();
    }
    for (auto& segs : segments_) segs.clear();
}

float TextRenderer::textWidth(std::string_view text, float sizePx) const {
    return font_.textWidth(text, sizePx);
}

void TextRenderer::cleanup() {
    renderer_.cleanup();
}
