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
#ifdef __ANDROID__
#include "activity_bridge.hh"
#endif
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
// The faces, all out of the shared `fonts` submodule (assets/fonts), staged
// beside the executable by the build.
//
// Song / Mincho / Batang: the Ming-Mincho-Myeongjo serif tradition, whose
// stroke contrast and terminal serifs read as one family with New Computer
// Modern's serif Latin. The sans cuts bundled beside them (Hei, Gothic, Dotum)
// are deliberately NOT registered — whichever face won a codepoint would decide
// the look, and a line would come out in mixed styles.
//
// Chinese -> Japanese -> Korean, and the order matters: all three cover Han and
// Kana, and only the Korean face has Hangul.
constexpr const char* kFaceRegular = "fonts/newcomputermodern/NewCM10-Regular.otf";
constexpr const char* kFaceBold    = "fonts/newcomputermodern/NewCM10-Bold.otf";
constexpr const char* kFaceItalic  = "fonts/newcomputermodern/NewCM10-Italic.otf";
constexpr const char* kFallbackCn  = "fonts/fandol/FandolSong-Regular.otf";
constexpr const char* kFallbackJp  = "fonts/haranoaji/HaranoAjiMincho-Regular.otf";
constexpr const char* kFallbackKr  = "fonts/unfonts-core/UnBatang.ttf";
constexpr const char* kFallbackCnB = "fonts/fandol/FandolSong-Bold.otf";
constexpr const char* kFallbackJpB = "fonts/haranoaji/HaranoAjiMincho-Bold.otf";
constexpr const char* kFallbackKrB = "fonts/unfonts-core/UnBatangBold.ttf";

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

// Should this app ask for an HDR swapchain at all?
//
// It used to ask unconditionally, and that produced a false claim: a wlroots
// compositor advertises the HDR10 ST 2084 format/colorspace pair whatever the
// monitor is, vk_canvas's pickTarget() only checks that the SURFACE offers the
// pair, and so hdrActive() came back true on a plain SDR display. The app then
// said "HDR active" while the compositor quietly tone-mapped our PQ back down
// — strictly worse than having rendered SDR in the first place.
//
// Nothing available here can tell us better. Knowing a Wayland output's real
// capability needs the colour-management protocol, which vk_canvas does not
// implement (its USAGE_hdr_output.md lists Wayland HDR as a non-goal). So the
// desktop default is SDR and HDR is an explicit opt-in, because a request we
// cannot verify is a claim we cannot make.
//
// Android is different and is left alone: the request there is the manifest
// meta-data app_shell reads, and the platform can be ASKED afterwards what the
// display really does — see refreshHeadroom().
bool DemoApp::resolveHdrRequest() {
#ifdef __ANDROID__
    return true;
#else
    const char* v = std::getenv("UI_DEMO_HDR");
    return v && std::strcmp(v, "0") != 0;
#endif
}

// How far above display white this panel can actually go, measured rather than
// assumed. 1.0 means "no headroom", which is a real and common answer.
//
// This is the number the clip-warning stripes are drawn against, so inventing
// it defeats the whole point of them: with a made-up 4.0 nothing between 1.0
// and 4.0 was striped, on a screen that could not show any of it.
void DemoApp::refreshHeadroom() {
    if (!hdr_) { headroom_ = 1.0f; headroomKnown_ = true; return; }
#ifdef __ANDROID__
    // Display.getHdrSdrRatio() where the platform has it, the static
    // HdrCapabilities otherwise; app_shell clamps both to >= 1.0.
    headroom_      = activity::display_hdr_headroom();
    headroomKnown_ = true;
#else
    // No way to ask a Wayland output. Report NO headroom rather than guess:
    // under-claiming shows stripes on highlights that might have been fine,
    // over-claiming hides ones that certainly are not.
    headroom_      = 1.0f;
    headroomKnown_ = false;
#endif
}

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
    hdrRequested_ = resolveHdrRequest();
    renderer_ = std::make_unique<Renderer>(
        host_->surfaceProvider(), host_->assetReader(), /*images=*/3,
        hdrRequested_ ? OutputTarget::Hdr10PQ : OutputTarget::SdrSrgb);

    // ASK. The request can be refused by the driver, by the compositor, or by
    // the window never having been put into HDR colour mode, and none of those
    // say so out loud. hdrActive() is the only honest answer to "what did the
    // SWAPCHAIN become" — and still not an answer to "what can the SCREEN do".
    hdr_          = renderer_->hdrActive();
    activeTarget_ = renderer_->activeTarget();
    refreshHeadroom();
    VCE_LOGI("ui_demo", "output: requested=%s target=%s hdr=%s headroom=%.2fx (%s)",
             hdrRequested_ ? "Hdr10PQ" : "SdrSrgb",
             outputTargetName(activeTarget_), hdr_ ? "yes" : "no",
             headroom_, headroomKnown_ ? "measured" : "not detectable here");
    hdrPage_.setOutput(activeTarget_, hdrRequested_, hdr_, headroom_, headroomKnown_);

    if (!buildUiFont()) {
        host_->showErrorMessage("ui_demo", "failed to open the UI faces (assets/fonts)");
        return false;
    }

    // The math page's font. atlas_gen's offline MTSDF bake is the only source
    // of OpenType MATH tables, so this one page keeps the older path; every
    // other page draws with uiFont_. Not fatal if it is missing — nine pages
    // still work, and the math page says so rather than the app refusing to
    // start.
    if (!mathFont_.load(host_->assetReader(), "fonts/math/font.msdf",
                        "fonts/math/atlas.rgba")) {
        VCE_LOGE("ui_demo", "no baked math atlas (assets/fonts/math); math page disabled");
        mathAvailable_ = false;
    }

    bindFont(uiFont_);
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
    imagePage_.setHeadroom(headroom_);
}

// Open the UI faces and register the fallback chains. Per STYLE, because a
// bold CJK label whose chain is missing silently resolves back to the Regular
// face: the text still renders, at the wrong weight, which looks like working
// software right up until it sits beside bold Latin.
bool DemoApp::buildUiFont() {
    AssetReader& r = host_->assetReader();
    if (!uiFont_.open(r, kFaceRegular)) return false;
    uiFont_.addStyle(r, kFaceBold,   FontStyle::Bold);
    uiFont_.addStyle(r, kFaceItalic, FontStyle::Italic);

    uiFont_.addFallback(r, kFallbackCn,  FontStyle::Roman);
    uiFont_.addFallback(r, kFallbackJp,  FontStyle::Roman);
    uiFont_.addFallback(r, kFallbackKr,  FontStyle::Roman);
    uiFont_.addFallback(r, kFallbackCnB, FontStyle::Bold);
    uiFont_.addFallback(r, kFallbackJpB, FontStyle::Bold);
    uiFont_.addFallback(r, kFallbackKrB, FontStyle::Bold);
    // Italic has no matched CJK cut in these families; a miss falls through to
    // the Roman chain, which is the right answer and not a gap.

    // Seed the atlas with printable ASCII at the sizes the pages actually use.
    //
    // Not an optimisation: an atlas with nothing in it has no pixels, and
    // binding it makes Renderer::initMsdf log "atlas pixels not resident" and
    // decline to build the pipeline. The miss path would fix that a frame
    // later, but starting from empty means starting from an error. Every size
    // beyond these still arrives through misses — that is the mechanism, and
    // this is only its first cell.
    std::vector<uint32_t> ascii;
    ascii.reserve(95);
    for (uint32_t cp = 0x20; cp <= 0x7E; ++cp) ascii.push_back(cp);
    const std::vector<int> sizes{14, 15, 16, 17, 18, 20, 22, 24, 26};
    (void)uiFont_.ensureGlyphs(ascii, sizes);
    return true;
}

void DemoApp::bindFont(TextFont& f) {
    if (bound_ == &f) return;
    bound_ = &f;
    // Two different fonts mean a different atlas shape, so this is the
    // expensive branch of initMsdf: it waits for in-flight frames and rebuilds.
    // Affordable because it happens on a page change, not per frame.
    renderer_->initMsdf(f);
}

void DemoApp::drainGlyphMisses() {
    if (!renderer_ || bound_ != &uiFont_) return;
    if (!uiFont_.hasMisses()) return;
    if (uiFont_.bakeMisses() > 0) {
        // Same font, grown atlas — initMsdf patches only the pages that
        // actually changed unless it had to add one.
        renderer_->initMsdf(uiFont_);
    }
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

    // The safe area, not the window. On both desktops these insets are zero and
    // this is the window rect; on a phone the camera is a hole punched through
    // the glass and nothing drawn under it can be read. Hiding the system bars
    // (which app_shell does) extends the window to the full panel and puts the
    // nav strip straight under the notch, which is exactly what this avoids.
    const SafeInsets insets = host_->safeInsets();
    Rect windowRect{(float)insets.left, (float)insets.top,
                    (float)renderer_->width()  - (float)(insets.left + insets.right),
                    (float)renderer_->height() - (float)(insets.top + insets.bottom)};

    UiScale uiScale{661.0f, 0.5f};
    float uiScaleFactor = uiScale.factor(windowRect.h);
    // Pages whose content stacks vertically (Text, Math) use a capped scale:
    // uncapped, their padding and gaps grow faster than the window and push the
    // bottom rows off-screen when maximized.
    float contentScaleFactor = uiScale.cappedFactor(windowRect.h, 1.6f);

    // The page is settled BEFORE the Canvas exists, because which font the
    // Canvas draws with depends on it: the math page needs the MTSDF face with
    // the MATH tables, everything else uses the raster UI face, and the
    // Renderer can only have one atlas bound at a time.
    nav_.updateLayout(windowRect, uiScaleFactor);
    currentPage_ = nav_.update(input(), currentPage_);
    if (currentPage_ == Page::Math && !mathAvailable_) currentPage_ = Page::Text;
    const bool wantMath = (currentPage_ == Page::Math);
    bindFont(wantMath ? static_cast<TextFont&>(mathFont_)
                      : static_cast<TextFont&>(uiFont_));

    Canvas canvas(curves_, renderer_->width(), renderer_->height(),
                  /*font=*/nullptr, 0.0f, 0.0f, 0.0f, 0.0f);
    canvas.useMsdf(bound_, &msdfQuads_);
    canvas.useShapes(&shapeVerts_);
    canvas.useImagesFg(&imagesFg_);

    // The page background, over the WHOLE surface including the insets: the
    // render pass clears to black, and letting that show under a notch reads as
    // a bug rather than as glass.
    canvas.rect(0, 0, (float)renderer_->width(), (float)renderer_->height(), gray(0.5f));

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
            MathCanvas mathCanvas(canvas, mathFont_);
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

    // After the frame, not before it: the misses are what THIS frame asked for
    // and could not draw. Baking them now means the next frame has them.
    drainGlyphMisses();
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
    renderer_ = std::make_unique<Renderer>(
        host_->surfaceProvider(), host_->assetReader(), /*images=*/3,
        hdrRequested_ ? OutputTarget::Hdr10PQ : OutputTarget::SdrSrgb);
    hdr_          = renderer_->hdrActive();
    activeTarget_ = renderer_->activeTarget();
    // The headroom can genuinely have CHANGED across this rebuild — an HDR
    // mode switch or a move to another display is exactly what invalidates the
    // pipelines and lands us here.
    refreshHeadroom();
    hdrPage_.setOutput(activeTarget_, hdrRequested_, hdr_, headroom_, headroomKnown_);
    imagePage_.setHeadroom(headroom_);
    VCE_LOGI("ui_demo", "output target after rebuild: %s (hdr=%s)",
             outputTargetName(activeTarget_), hdr_ ? "yes" : "no");
    // A new Renderer means new pipelines and a new atlas texture, so this one
    // does need the upload. Both fonts are CPU-side and outlived all of it;
    // force a rebind so the currently-shown page's font goes back up.
    TextFont* was = bound_;
    bound_ = nullptr;
    bindFont(was == &mathFont_ ? static_cast<TextFont&>(mathFont_)
                               : static_cast<TextFont&>(uiFont_));
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
