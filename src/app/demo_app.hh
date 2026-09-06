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
    // Bake any codepoint in `utf8` the atlas does not already carry, and
    // re-upload. Cheap when nothing was missing, which is the common case.
    void ensureGlyphs(const std::string& utf8);

    std::unique_ptr<Host>     host_;
    std::unique_ptr<Renderer> renderer_;
    MsdfFont                  font_;

    // Whether the HDR swapchain request was actually granted. Asked once, at
    // create(), and never assumed — see page_hdr.h for why the honest answer
    // matters more here than in most apps.
    bool         hdr_          = false;
    OutputTarget activeTarget_ = OutputTarget::SdrSrgb;

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
