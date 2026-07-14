#include "platform/windows/window.h"
#include "platform/windows/vk_surface_windows.h"
#include "platform/fatal.h"
#include "gfx/vk_core.h"
#include "gfx/shape_pipeline.h"
#include "text/text_renderer.h"
#include "nav.h"
#include "page_text.h"
#include "page_shapes.h"
#include "page_widgets.h"
#include "page_animation.h"
#include "page_math.h"
#include "../math/math_canvas.h"
#include "../ui/layout.h"
#include "../anim/animated_float.h"
#include <chrono>
#include <cmath>
#include <thread>

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    Window window;
    if (!window.create(hInst, 1280, 800, L"windows_ui_demo")) return 1;

    VkCore vk;
    vk.createInstance({VK_KHR_WIN32_SURFACE_EXTENSION_NAME});
    VkSurfaceKHR surface = createWin32Surface(vk.instance(), hInst, window.hwnd());
    if (!vk.init(surface, 1280, 800)) return 1;

    ShapePipeline shapes;
    TextRenderer text;
    bool shapesReady = false;
    bool textReady = false;

    // Font baking (msdfgen/FreeType rasterization, CPU-only) is the slow
    // part of startup (10-20s on a cache miss). Run it on a background
    // thread so window.pumpMessages() keeps being serviced every frame and
    // Windows never shows the busy cursor / "Not Responding" state. Only
    // CPU-only work happens on this thread (TextRenderer::bakeFonts()) --
    // no Vulkan calls are ever made off the main thread.
    std::thread fontThread(&TextRenderer::bakeFonts, &text,
        std::vector<std::string>{kChineseSample, kJapaneseSample, kKoreanSample});

    PrimitiveBatch batch;
    TopNav nav;
    TextPage textPage;
    ShapesPage shapesPage;
    WidgetsPage widgetsPage;
    AnimationPage animationPage;
    MathPage mathPage;
    Page currentPage = Page::Text;

    // Simple non-text loading indicator (a pulsing rounded rect) shown only
    // while textReady is false, so the window isn't just a static gray
    // rectangle during the background bake.
    AnimatedFloat pulse(0.3f);
    pulse.set(0.7f, 0.6f, easeInOutCubic);

    while (window.pumpMessages()) {
        int w, h;
        if (window.consumeResized(w, h)) vk.notifyResize(w, h);

        FrameContext frame;
        if (vk.beginFrame(frame)) {
            if (!shapesReady) {
                shapes.init(vk.device(), vk.physicalDevice(), frame.renderPass);
                shapesReady = true;
            }
            if (!textReady && text.fontsBaked()) {
                if (!text.bakeSucceeded()) fatal("Font baking failed (bad/missing font file?)");
                text.finishGpuInit(vk.device(), vk.physicalDevice(), frame.renderPass,
                                   frame.extent.width, frame.extent.height);
                text.recordAtlasUpload(frame.cmd);
                textReady = true;
            }

            Rect windowRect{0, 0, (float)frame.extent.width, (float)frame.extent.height};
            UiScale uiScale{661.0f, 0.5f};
            float uiScaleFactor = uiScale.factor(windowRect.h);
            // Pages whose content stacks vertically (Text, Math) use a capped
            // scale: uncapped, their padding/gaps grow faster than the window
            // and push the bottom rows off-screen on a maximized window. The
            // cap plus wheel scrolling (see those pages) keeps everything
            // reachable.
            float contentScaleFactor = uiScale.cappedFactor(windowRect.h, 1.6f);

            batch.reset();

            if (textReady) {
                nav.updateLayout(windowRect, uiScaleFactor);
                currentPage = nav.update(window.input(), currentPage);

                nav.draw(batch, text, currentPage);
                switch (currentPage) {
                    case Page::Text:
                        textPage.update(window.input(), nav.contentArea());
                        textPage.draw(batch, text, nav.contentArea(), contentScaleFactor);
                        break;
                    case Page::Shapes:
                        shapesPage.draw(batch, nav.contentArea(), uiScaleFactor);
                        break;
                    case Page::Widgets:
                        widgetsPage.updateLayout(nav.contentArea(), uiScaleFactor);
                        widgetsPage.update(window.input());
                        widgetsPage.draw(batch, text);
                        break;
                    case Page::Animation: {
                        static auto lastTick = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        float dt = std::chrono::duration<float>(now - lastTick).count();
                        lastTick = now;
                        animationPage.updateLayout(nav.contentArea(), uiScaleFactor);
                        animationPage.update(dt, window.input());
                        animationPage.draw(batch, text);
                        break;
                    }
                    case Page::Math: {
                        static auto lastTick = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        float dt = std::chrono::duration<float>(now - lastTick).count();
                        lastTick = now;
                        mathPage.update(dt, window.input(), nav.contentArea());
                        MathCanvas canvas(batch, text);
                        mathPage.draw(canvas, nav.contentArea(), contentScaleFactor);
                        break;
                    }
                }

                // Hand cursor over anything clickable (nav tabs everywhere;
                // the demo controls on the Widgets page), arrow otherwise.
                bool wantHand = nav.hoversAnyTab(window.input()) ||
                    (currentPage == Page::Widgets && widgetsPage.hoversAnyWidget(window.input()));
                window.setDesiredCursor(wantHand ? CursorShape::Hand : CursorShape::Arrow);
            } else {
                // Fonts are still baking on the background thread -- draw a
                // small pulsing indicator instead of any text-touching
                // content (nav/pages all draw label text via `text`, which
                // isn't GPU-initialized yet).
                static auto lastTick = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - lastTick).count();
                lastTick = now;
                pulse.update(dt);
                if (!pulse.isAnimating()) {
                    float next = (pulse.value() > 0.5f) ? 0.3f : 0.7f;
                    pulse.set(next, 0.6f, easeInOutCubic);
                }
                float cx = windowRect.w * 0.5f, cy = windowRect.h * 0.5f;
                batch.pushRoundedRect(cx - 40.0f, cy - 10.0f, 80.0f, 20.0f, 6.0f, pulse.value());
            }

            if (shapesReady) shapes.draw(frame.cmd, frame.extent, batch);
            if (textReady) text.draw(frame.cmd, frame.extent);
            vk.endFrame();
        }
    }
    // The background bake may still be running if the window was closed
    // early -- std::thread must be joined before text/vk (which the thread
    // may still be touching via font_/assets_) get destroyed, otherwise
    // ~thread() calls std::terminate() if still joinable.
    if (fontThread.joinable()) fontThread.join();

    if (textReady) text.cleanup();
    if (shapesReady) shapes.cleanup(vk.device());
    vk.cleanup();
    return 0;
}
