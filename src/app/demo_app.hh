// demo_app.hh — the whole application, with no platform in it.
//
// What used to be main.cpp's wWinMain: a window, a wnd_proc, a PeekMessage
// loop and the frame body, all in one file that only Windows could compile.
// app_shell splits that in two. The Host owns the window and the pump; this
// owns the renderer, the font, the pages, and what to draw — and it names no
// OS at all, which is the only reason the same class runs on Wayland and on a
// phone.
//
// It derives FrameInputView rather than AppView directly: the pages read a
// vk_canvas FrameInput, and FrameInputView is app_shell's adapter that turns
// the AppView callbacks (a pointer moved, a key went down, an IME replaced the
// buffer) into exactly that. Everything the old wnd_proc did by hand.
#pragma once

#include <chrono>
#include <string>
#include <memory>
#include <vector>

#include "frame_input_view.hh"
#include "host.hh"

#include "canvas.hh"
#include "msdf.hh"
#include "raster_font.hh"
#include "texture.hh"
#include "output_target.hh"
#include "renderer.hh"

#include "nav.h"
#include "page_animation.h"
#include "page_gestures.h"
#include "page_hdr.h"
#include "page_image.h"
#include "page_math.h"
#include "page_shapes.h"
#include "page_plot.h"
#include "page_text.h"
#include "page_textedit.h"
#include "page_widgets.h"

class DemoApp : public FrameInputView {
 public:
    // The host is injected rather than constructed here, because android_main
    // is handed one built from its android_app* and the desktop entry builds
    // its own. app_shell's rule that create()/run() belong to the APP is what
    // makes both callers look the same from in here.
    explicit DemoApp(std::unique_ptr<Host> host);
    ~DemoApp() override;

    bool create();
    void run();

    // ── AppView ─────────────────────────────────────────────────────────────
    void onHostResized() override;
    void shutdown() override;
    void onHostExposed() override;
    void onHostLayoutInvalidated() override;

    // The CPU keeps the font and every page's state; only the Vulkan objects
    // die with the surface. On a phone that is every trip through the recents
    // switcher, and the split is invisible until the second visit.
    void onSurfaceLost() override;
    bool onSurfaceRecreated() override;

 private:
    void draw(float dt);
    void uploadTextures();
    bool buildUiFont();
    // Upload `f`'s atlas and make it the one the Renderer samples. A no-op
    // when it is already bound, so this is safe to call every frame.
    void bindFont(TextFont& f);
    // Rasterize whatever the last frame asked for and did not have. A per-size
    // cache cannot know every size in advance, so it learns them from what is
    // actually drawn: one frame of a missing glyph the first time a size
    // appears, and nothing afterwards.
    void drainGlyphMisses();

    std::unique_ptr<Host>     host_;
    std::unique_ptr<Renderer> renderer_;

    // TWO fonts, and the split is forced by the engine rather than chosen.
    //
    // uiFont_ is a RasterFont: the current path. Faces are opened straight
    // from the shared `fonts` submodule at runtime, per-style fallback chains
    // serve CJK, and glyphs are rasterized lazily per size. No offline bake,
    // no atlas cache, and nothing to keep in step.
    //
    // mathFont_ is an MsdfFont loaded from atlas_gen's offline MTSDF bake, and
    // it exists for exactly one page. OpenType MATH tables — MathConstants,
    // the variant/assembly constructions, buildVStretch — are produced ONLY by
    // that offline bake (msdf.cc sets hasMath_ in load() and nowhere else), and
    // the TextFont seam every other font goes through has no notion of math at
    // all. Matrix Player gets to be pure RasterFont because it never renders
    // math; this demo does, so the old path survives here for that one page.
    //
    // The Renderer holds ONE atlas, so they are swapped on page change rather
    // than mixed — see bindFont().
    RasterFont                uiFont_;
    MsdfFont                  mathFont_;
    TextFont*                 bound_ = nullptr;
    // False when the offline math atlas is absent. Nine pages do not need it,
    // so the app runs without one rather than refusing to start.
    bool                      mathAvailable_ = true;

    // Three DIFFERENT facts that were previously conflated into one:
    //
    //   hdrRequested_ — did this app even ask?
    //   hdr_          — did we get an HDR-encoded swapchain? (hdrActive())
    //   headroom_     — how much brighter than white THIS DISPLAY can actually
    //                   go, measured. 1.0 means none.
    //
    // The second does not imply the third, and assuming it does is the bug
    // this page was reported for: a compositor happily hands out an HDR10 PQ
    // swapchain on a display with no headroom at all, and then tone-maps it
    // back down. See resolveHdrRequest() and refreshHeadroom().
    bool         hdrRequested_  = false;
    bool         hdr_           = false;
    bool         headroomKnown_ = false;
    float        headroom_      = 1.0f;
    OutputTarget activeTarget_  = OutputTarget::SdrSrgb;

    static bool  resolveHdrRequest();
    void         refreshHeadroom();

    TopNav        nav_;
    TextPage      textPage_;
    ShapesPage    shapesPage_;
    WidgetsPage   widgetsPage_;
    AnimationPage animationPage_;
    MathPage      mathPage_;
    HdrPage       hdrPage_;
    ImagePage     imagePage_;
    GesturesPage  gesturesPage_;
    TextEditPage  textEditPage_;
    PlotPage      plotPage_;
    Page          currentPage_ = Page::Text;

    // Stays empty every frame: every primitive rides the library's SDF shape
    // quad path, so the compute rasterizer's screen-sized buffers are never
    // allocated and frames fully overlap.
    std::vector<float> curves_;
    std::vector<float> msdfQuads_;
    std::vector<float> shapeVerts_;
    // The HDR page draws through the image path, which is the only path where
    // a value above display white survives the output encode.
    std::vector<ImageDraw> imagesFg_;
    TextureHandle          hdrRamp_   = kInvalidTexture;
    TextureHandle          sceneTex_  = kInvalidTexture;

    bool running_    = true;
    bool surfaceOk_  = false;
    std::chrono::steady_clock::time_point lastTick_{};
};
