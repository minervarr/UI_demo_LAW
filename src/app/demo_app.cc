// demo_app.cc — the frame loop that used to be wWinMain's body.
#include "demo_app.hh"

#include <algorithm>
#include <initializer_list>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#include "layout.hh"
#include "log.hh"
#include "utf8.hh"

#include "gray.h"
#include "../math/math_canvas.h"

namespace {

// The offline-baked atlas covers ASCII/Latin-1 (Roman/Bold/Italic) + the math
// sets, but not CJK: bake the showcase strings' codepoints from the bundled
// fallback fonts into the same shared atlas, BEFORE Renderer::initMsdf()
// uploads it.
//
// One bakeCodepoints() call per fallback font with the WHOLE missing list,
// never one call per codepoint: each call appends new atlas row(s), and the
// offline atlas (3072x3258) leaves only ~800px of headroom under the engine's
// 4096 guaranteed-texture-size cap — per-codepoint calls burn a full ~100px
// row per glyph and run out after ~8 glyphs (observed), while one batched call
// packs all ~16 showcase glyphs into a single row. bakeCodepoints itself skips
// codepoints a font doesn't cover (leaving them for the next font) and ones
// already resolved (so later fonts don't re-bake).
const char* const kFallbackFonts[] = {
    "fonts/fandol/FandolHei-Regular.otf",
    "fonts/haranoaji/HaranoAjiGothic-Regular.otf",
    "fonts/unfonts-core/UnDotum.ttf",
};

// Returns true if anything was baked, so the caller knows whether the atlas
// needs re-uploading.
bool bakeMissing(MsdfFont& font, AssetReader& assets,
                 std::initializer_list<std::string_view> samples) {
    std::vector<uint32_t> missing;
    for (std::string_view text : samples) {
        size_t i = 0;
        while (i < text.size()) {
            uint32_t cp = utf8::nextCodepoint(text, i);
            if (font.hasCodepoint(cp)) continue;
            if (std::find(missing.begin(), missing.end(), cp) == missing.end())
                missing.push_back(cp);
        }
    }
    if (missing.empty()) return false;
    for (const char* rel : kFallbackFonts)
        font.bakeCodepoints(assets, rel, missing);
    return true;
}

// Which page to open on. Exists so a screenshot of one page can be taken
// without a human clicking a tab first — every page but the default is
// otherwise unreachable from a script, and rendering here is verified by
// looking at it. Unset means Text, exactly as before.
Page startPage() {
    const char* v = std::getenv("UI_DEMO_PAGE");
    if (!v) return Page::Text;
    struct { const char* name; Page page; } kNames[] = {
        {"text", Page::Text}, {"shapes", Page::Shapes}, {"widgets", Page::Widgets},
        {"animation", Page::Animation}, {"math", Page::Math}, {"hdr", Page::Hdr},
        {"image", Page::Image}, {"gestures", Page::Gestures},
        {"textedit", Page::TextEdit}, {"plot", Page::Plot},
    };
    for (const auto& n : kNames)
        if (std::strcmp(v, n.name) == 0) return n.page;
    return Page::Text;
}

}  // namespace

DemoApp::DemoApp(std::unique_ptr<Host> host) : host_(std::move(host)) {}
DemoApp::~DemoApp() = default;

bool DemoApp::create() {
    if (!host_ || !host_->init(this)) return false;

    // Hdr10PQ, requested — never assumed. vk_canvas resolves this against what
    // the surface actually enumerates and falls back to the SDR pin when the
    // pair is not there, so asking costs nothing on a device that cannot do it.
    //
    // PQ rather than scRGB because PQ is what a phone actually grants: the
    // 10-bit ST 2084 pair is what Android surfaces enumerate once the window is
    // in HDR colour mode, and its absolute luminance means the ramp on the HDR
    // page stands for real nits instead of a number scaled by whatever the OS
    // currently calls SDR white. The accepted cost is that the fixed-function
    // blend mixes PQ code values rather than the luminances they encode, so
    // antialiased edges are very slightly wrong — a sub-pixel error, paid for
    // correct absolute brightness. See vk_canvas USAGE_hdr_output.md.
    renderer_ = std::make_unique<Renderer>(host_->surfaceProvider(), host_->assetReader(),
                                           /*images=*/3, OutputTarget::Hdr10PQ);

    // ASK. The request can be refused by the driver, by the compositor, or by
    // the window never having been put into HDR colour mode, and none of those
    // say so out loud. hdrActive() is the only honest answer.
    hdr_          = renderer_->hdrActive();
    activeTarget_ = renderer_->activeTarget();
    VCE_LOGI("ui_demo", "output target: %s (hdr=%s)",
             outputTargetName(activeTarget_), hdr_ ? "yes" : "no");
    hdrPage_.setOutput(activeTarget_, hdr_);

    // One MsdfFont for everything: the build-time atlas_gen bake packs
    // NewComputerModern Roman/Bold/Italic + the MATH face (glyphs AND OpenType
    // MATH metrics) into a single atlas, loaded here in milliseconds.
    if (!font_.load(host_->assetReader(), "fonts/math/font.msdf", "fonts/math/atlas.rgba")) {
        host_->showErrorMessage("ui_demo",
                                "failed to load the baked font atlas (assets/fonts/math)");
        return false;
    }
    bakeMissing(font_, host_->assetReader(),
                {kChineseSample, kJapaneseSample, kKoreanSample,
                 TextEditPage::kSeedText});
    renderer_->initMsdf(font_);
    uploadTextures();

    // The text-entry page needs the soft keyboard raised and dismissed, and a
    // page has no Host — so it gets function pointers instead of one. Empty on
    // desktop in effect: Host::showKeyboard/hideKeyboard have do-nothing
    // defaults there, and the page never learns which case it is in.
    textEditPage_.setKeyboardHooks(
        this,
        [](void* ctx, const std::string& text, size_t cursorByte) {
            static_cast<DemoApp*>(ctx)->host_->showKeyboard(text, cursorByte);
        },
        [](void* ctx) { static_cast<DemoApp*>(ctx)->host_->hideKeyboard(); });
    textEditPage_.setGlyphHook(
        this, [](void* ctx, const std::string& text) {
            static_cast<DemoApp*>(ctx)->ensureGlyphs(text);
        });

    currentPage_ = startPage();
    surfaceOk_   = true;
    lastTick_    = std::chrono::steady_clock::now();
    host_->showWindow();
    return true;
}

// Both textures are linear-light RGBA32F. RGBA16F would halve the bytes but
// would need a float-to-half converter in here; these are 512x8 and 384x256, so
// the difference is not worth the code.
//
// mips=false on the ramp: it is never minified, and a mip chain would average
// away exactly the bright end the page exists to show.
void DemoApp::uploadTextures() {
    if (!renderer_) return;

    const std::vector<float> ramp = HdrPage::rampPixels();
    hdrRamp_ = renderer_->create_texture(
        reinterpret_cast<const uint8_t*>(ramp.data()),
        (uint32_t)HdrPage::kRampW, (uint32_t)HdrPage::kRampH,
        /*mips=*/false, TextureFormat::RGBA32F);
    hdrPage_.setRampTexture(hdrRamp_);

    const std::vector<float> scene = ImagePage::scenePixels();
    sceneTex_ = renderer_->create_texture(
        reinterpret_cast<const uint8_t*>(scene.data()),
        (uint32_t)ImagePage::kTexW, (uint32_t)ImagePage::kTexH,
        /*mips=*/false, TextureFormat::RGBA32F);
    imagePage_.setTexture(sceneTex_);
    imagePage_.setHdr(hdr_);
}

// The atlas is baked at startup from strings known then. Anything typed later
// — and on Android that means anything an IME produces — is by definition not
// in it, and a glyph the atlas lacks renders as nothing at all: the row goes
// blank rather than wrong, which is the hard kind to notice.
//
// initMsdf() is built to be called again: when the atlas kept its shape it
// patches only the pages that changed, and when a bake grew it by a row it
// rebuilds. So the fix is simply to bake and re-upload, and to do it only when
// something was actually missing.
void DemoApp::ensureGlyphs(const std::string& utf8) {
    if (!renderer_ || utf8.empty()) return;
    if (bakeMissing(font_, host_->assetReader(), {std::string_view(utf8)}))
        renderer_->initMsdf(font_);
}

void DemoApp::run() {
    while (running_) {
        // Clear last frame's edges BEFORE pumping, exactly as the old loop did
        // — beginFrame() is what makes "went down this frame" mean anything.
        beginFrame();
        // Animation and the HDR ramp both want a continuous clock, so this app
        // always has work. A phone still throttles it: the host blocks on the
        // compositor when the window is not visible.
        host_->pump(/*haveWork=*/true);
        if (host_->quitRequested()) break;
        if (!running_) break;

        auto  now = std::chrono::steady_clock::now();
        float dt  = std::chrono::duration<float>(now - lastTick_).count();
        lastTick_ = now;
        // A pump that blocked for seconds (backgrounded, or the surface gone)
        // must not arrive as one enormous animation step.
        if (dt > 0.25f) dt = 0.25f;

        if (surfaceOk_ && renderer_) draw(dt);
    }
}

void DemoApp::draw(float dt) {
    msdfQuads_.clear();
    shapeVerts_.clear();
    imagesFg_.clear();

    Canvas canvas(curves_, renderer_->width(), renderer_->height(),
                  /*font=*/nullptr, 0.0f, 0.0f, 0.0f, 0.0f);
    canvas.useMsdf(&font_, &msdfQuads_);
    canvas.useShapes(&shapeVerts_);
    canvas.useImagesFg(&imagesFg_);

    // The safe area, not the window. On both desktops these insets are zero and
    // this is the window rect; on a phone the camera is a hole punched through
    // the glass and nothing drawn under it can be read. Hiding the system bars
    // (which app_shell does) extends the window to the full panel and puts the
    // nav strip straight under the notch, which is exactly what this avoids.
    const SafeInsets insets = host_->safeInsets();
    Rect windowRect{(float)insets.left, (float)insets.top,
                    (float)renderer_->width()  - (float)(insets.left + insets.right),
                    (float)renderer_->height() - (float)(insets.top + insets.bottom)};

    // Mid-gray background over the WHOLE surface, insets included: the render
    // pass clears to black, and letting that show through under the notch
    // would draw a black band the user reads as a bug rather than as glass.
    canvas.rect(0, 0, (float)renderer_->width(), (float)renderer_->height(), gray(0.5f));

    UiScale uiScale{661.0f, 0.5f};
    float uiScaleFactor = uiScale.factor(windowRect.h);
    // Pages whose content stacks vertically (Text, Math) use a capped scale:
    // uncapped, their padding and gaps grow faster than the window and push the
    // bottom rows off-screen when maximized.
    float contentScaleFactor = uiScale.cappedFactor(windowRect.h, 1.6f);

    nav_.updateLayout(windowRect, uiScaleFactor);
    currentPage_ = nav_.update(input(), currentPage_);
    nav_.draw(canvas, currentPage_);

    const Rect content = nav_.contentArea();
    switch (currentPage_) {
        case Page::Text:
            textPage_.update(input(), content);
            textPage_.draw(canvas, content, contentScaleFactor);
            break;
        case Page::Shapes:
            shapesPage_.draw(canvas, content, uiScaleFactor);
            break;
        case Page::Widgets:
            widgetsPage_.updateLayout(content, uiScaleFactor);
            widgetsPage_.update(input());
            widgetsPage_.draw(canvas);
            break;
        case Page::Animation:
            animationPage_.updateLayout(content, uiScaleFactor);
            animationPage_.update(dt, input());
            animationPage_.draw(canvas);
            break;
        case Page::Math: {
            mathPage_.update(dt, input(), content);
            MathCanvas mathCanvas(canvas, font_);
            mathPage_.draw(mathCanvas, content, contentScaleFactor);
            break;
        }
        case Page::Hdr:
            hdrPage_.updateLayout(content, uiScaleFactor);
            hdrPage_.update(dt, input());
            hdrPage_.draw(canvas);
            break;
        case Page::Image:
            imagePage_.updateLayout(content, uiScaleFactor);
            imagePage_.update(dt, input());
            imagePage_.draw(canvas);
            break;
        case Page::Gestures:
            gesturesPage_.updateLayout(content, uiScaleFactor);
            gesturesPage_.update(dt, input());
            gesturesPage_.draw(canvas);
            break;
        case Page::TextEdit:
            textEditPage_.updateLayout(content, uiScaleFactor);
            textEditPage_.update(dt, input());
            textEditPage_.draw(canvas);
            break;
        case Page::Plot:
            plotPage_.updateLayout(content, uiScaleFactor);
            plotPage_.update(dt, input());
            plotPage_.draw(canvas);
            break;
    }

    // The pointer image follows hover state. On Android there is no pointer and
    // the host ignores this; asking anyway keeps the call site platform-free.
    const bool wantHand = nav_.hoversAnyTab(input()) ||
        (currentPage_ == Page::Widgets  && widgetsPage_.hoversAnyWidget(input())) ||
        (currentPage_ == Page::Hdr      && hdrPage_.hoversAnyWidget(input())) ||
        (currentPage_ == Page::Image    && imagePage_.hoversAnyWidget(input())) ||
        (currentPage_ == Page::Plot     && plotPage_.hoversAnyWidget(input())) ||
        (currentPage_ == Page::TextEdit && textEditPage_.hoversAnyWidget(input()));
    host_->setCursor(wantHand ? CursorShape::Hand : CursorShape::Arrow);

    renderer_->draw(curves_, /*overlay_rotation_deg=*/0, {}, imagesFg_, msdfQuads_, shapeVerts_);
}

void DemoApp::onHostResized() {
    // No layout to recompute: every page derives its own from the current
    // window size on each frame, so the next draw() already has the new one.
    // The swapchain does need telling, though — a Wayland configure is not
    // something a present call would have reported as OUT_OF_DATE.
    if (renderer_) renderer_->notifyResized();
}

void DemoApp::onHostLayoutInvalidated() {}
void DemoApp::onHostExposed() {}

void DemoApp::onSurfaceLost() {
    // Stop drawing, but keep the font and every page's state — the CPU side
    // survives, the GPU side does not.
    surfaceOk_ = false;
}

bool DemoApp::onSurfaceRecreated() {
    if (!renderer_) return false;

    // The cheap path, and the one that almost always runs: a new VkSurfaceKHR
    // over the same device. The instance, the device, the render pass, every
    // pipeline AND the glyph atlas all survive it, so there is deliberately no
    // initMsdf() here — re-uploading an atlas that never died is how you pay
    // the ~370 ms this call exists to avoid.
    if (renderer_->recreate_surface()) {
        surfaceOk_ = true;
        lastTick_  = std::chrono::steady_clock::now();
        return true;
    }

    // It returns false when the new surface is not compatible with pipelines
    // baked against the old one — a different format or colour space. An HDR
    // mode change is exactly that, and this app asks for an HDR swapchain, so
    // the case is reachable here rather than theoretical. Rebuild whole.
    VCE_LOGI("ui_demo", "surface incompatible with existing pipelines; rebuilding the renderer");
    renderer_.reset();
    renderer_ = std::make_unique<Renderer>(host_->surfaceProvider(), host_->assetReader(),
                                           /*images=*/3, OutputTarget::Hdr10PQ);
    hdr_          = renderer_->hdrActive();
    activeTarget_ = renderer_->activeTarget();
    hdrPage_.setOutput(activeTarget_, hdr_);
    VCE_LOGI("ui_demo", "output target after rebuild: %s (hdr=%s)",
             outputTargetName(activeTarget_), hdr_ ? "yes" : "no");
    // A new Renderer means new pipelines and a new atlas texture, so this one
    // does need the upload. font_ is the CPU copy and outlived all of it.
    renderer_->initMsdf(font_);
    // The old texture handle belonged to the Renderer that just died.
    hdrRamp_  = kInvalidTexture;
    sceneTex_ = kInvalidTexture;
    uploadTextures();
    surfaceOk_ = true;
    lastTick_  = std::chrono::steady_clock::now();
    return true;
}

void DemoApp::shutdown() {
    running_   = false;
    surfaceOk_ = false;
    renderer_.reset();
}
