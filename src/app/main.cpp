// Win32 entry point: raw windows.h window + message pump driving vk_canvas
// (libs/firstparty/vk_canvas) — the Renderer/Canvas/FrameInput/layout stack
// all come from the library; this app owns only the pages and their glue.

#include "win32_platform.hh"

#include <algorithm>
#include <chrono>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "canvas.hh"
#include "frame_input.hh"
#include "layout.hh"
#include "msdf.hh"
#include "renderer.hh"
#include "utf8.hh"

#include "gray.h"
#include "nav.h"
#include "page_animation.h"
#include "page_math.h"
#include "page_shapes.h"
#include "page_text.h"
#include "page_widgets.h"
#include "../math/math_canvas.h"

namespace {

bool g_running = true;
FrameInput g_input;
// Set by the frame loop when the pointer is over anything clickable; read by
// WM_SETCURSOR so the hand/arrow cursor follows hover state.
bool g_wantHand = false;

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (win32_translate_input(hwnd, msg, wp, lp, g_input)) {
        if (msg == WM_KEYDOWN && wp == VK_ESCAPE) { g_running = false; PostQuitMessage(0); }
        return 0;
    }
    switch (msg) {
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, g_wantHand ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        case WM_CLOSE:
        case WM_DESTROY:
            g_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            // No explicit action needed: Renderer::draw() discovers the
            // resize via OUT_OF_DATE/SUBOPTIMAL and recreates the swapchain
            // from Win32SurfaceProvider::extent() (live GetClientRect).
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

HWND create_window(uint32_t w, uint32_t h) {
    HINSTANCE hinst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = hinst;
    wc.hCursor       = nullptr;  // WM_SETCURSOR picks hand/arrow per frame
    wc.lpszClassName = L"windows_ui_demo_window";
    RegisterClassExW(&wc);

    DWORD style = WS_OVERLAPPEDWINDOW;  // resizable
    RECT rc{0, 0, (LONG)w, (LONG)h};
    AdjustWindowRect(&rc, style, FALSE);

    return CreateWindowExW(0, wc.lpszClassName, L"windows_ui_demo", style,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           rc.right - rc.left, rc.bottom - rc.top,
                           nullptr, nullptr, hinst, nullptr);
}

// The offline-baked atlas covers ASCII/Latin-1 (Roman/Bold/Italic) + the math
// sets, but not CJK: bake the showcase strings' codepoints from the bundled
// fallback fonts into the same shared atlas, BEFORE Renderer::initMsdf()
// uploads it. A handful of glyphs — fast enough to do synchronously at
// startup (no background bake thread needed with the pre-baked atlas).
//
// One bakeCodepoints() call per fallback font with the WHOLE missing list,
// never one call per codepoint: each call appends new atlas row(s), and the
// offline atlas (3072x3258) leaves only ~800px of headroom under the engine's
// 4096 guaranteed-texture-size cap — per-codepoint calls burn a full ~100px
// row per glyph and run out after ~8 glyphs (observed), while one batched
// call packs all ~16 showcase glyphs into a single row. bakeCodepoints
// itself skips codepoints a font doesn't cover (leaving them for the next
// font) and codepoints already resolved (so later fonts don't re-bake).
void bakeCjkFallbacks(MsdfFont& font, AssetReader& assets,
                      std::initializer_list<const char*> samples) {
    static const char* kFallbackFonts[] = {
        "fonts/fandol/FandolHei-Regular.otf",
        "fonts/haranoaji/HaranoAjiGothic-Regular.otf",
        "fonts/unfonts-core/UnDotum.ttf",
    };
    std::vector<uint32_t> missing;
    for (const char* sample : samples) {
        std::string_view text(sample);
        size_t i = 0;
        while (i < text.size()) {
            uint32_t cp = utf8::nextCodepoint(text, i);
            if (font.hasCodepoint(cp)) continue;
            if (std::find(missing.begin(), missing.end(), cp) == missing.end())
                missing.push_back(cp);
        }
    }
    if (missing.empty()) return;
    for (const char* rel : kFallbackFonts)
        font.bakeCodepoints(assets, rel, missing);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    HWND hwnd = create_window(1280, 800);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOW);

    Win32SurfaceProvider surface(hwnd);
    FileAssetReader      assets;

    try {
        Renderer renderer(surface, assets, /*desiredSwapchainImages=*/3);

        // One MsdfFont for everything: the build-time atlas_gen bake packs
        // NewComputerModern Roman/Bold/Italic + the MATH face (glyphs AND
        // OpenType MATH metrics) into a single atlas, loaded here in
        // milliseconds — no runtime msdfgen rasterization for the core set.
        MsdfFont font;
        if (!font.load(assets, "fonts/math/font.msdf", "fonts/math/atlas.rgba"))
            throw std::runtime_error("failed to load the baked font atlas (assets/fonts/math)");
        bakeCjkFallbacks(font, assets, {kChineseSample, kJapaneseSample, kKoreanSample});
        renderer.initMsdf(font);

        TopNav nav;
        TextPage textPage;
        ShapesPage shapesPage;
        WidgetsPage widgetsPage;
        AnimationPage animationPage;
        MathPage mathPage;
        Page currentPage = Page::Text;

        std::vector<float> curves;      // stays empty: everything rides the SDF
                                        // shape path, so the compute rasterizer
                                        // never allocates and frames overlap
        std::vector<float> msdfQuads;
        std::vector<float> shapeVerts;

        auto lastTick = std::chrono::steady_clock::now();
        while (g_running) {
            g_input.beginFrame();  // clear last frame's edges BEFORE pumping
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);  // generates WM_CHAR for the math editor
                DispatchMessageW(&msg);
            }
            if (!g_running) break;

            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - lastTick).count();
            lastTick = now;

            msdfQuads.clear();
            shapeVerts.clear();
            Canvas canvas(curves, renderer.width(), renderer.height(),
                          /*font=*/nullptr, 0.0f, 0.0f, 0.0f, 0.0f);
            canvas.useMsdf(&font, &msdfQuads);
            canvas.useShapes(&shapeVerts);

            Rect windowRect{0, 0, (float)renderer.width(), (float)renderer.height()};
            // Mid-gray background (the render pass clears to black): one
            // full-screen shape quad, same visual as the old demo's clear.
            canvas.rect(windowRect.x, windowRect.y, windowRect.w, windowRect.h, gray(0.5f));

            UiScale uiScale{661.0f, 0.5f};
            float uiScaleFactor = uiScale.factor(windowRect.h);
            // Pages whose content stacks vertically (Text, Math) use a capped
            // scale: uncapped, their padding/gaps grow faster than the window
            // and push the bottom rows off-screen when maximized. The cap
            // plus wheel scrolling keeps everything reachable.
            float contentScaleFactor = uiScale.cappedFactor(windowRect.h, 1.6f);

            nav.updateLayout(windowRect, uiScaleFactor);
            currentPage = nav.update(g_input, currentPage);
            nav.draw(canvas, currentPage);

            switch (currentPage) {
                case Page::Text:
                    textPage.update(g_input, nav.contentArea());
                    textPage.draw(canvas, nav.contentArea(), contentScaleFactor);
                    break;
                case Page::Shapes:
                    shapesPage.draw(canvas, nav.contentArea(), uiScaleFactor);
                    break;
                case Page::Widgets:
                    widgetsPage.updateLayout(nav.contentArea(), uiScaleFactor);
                    widgetsPage.update(g_input);
                    widgetsPage.draw(canvas);
                    break;
                case Page::Animation:
                    animationPage.updateLayout(nav.contentArea(), uiScaleFactor);
                    animationPage.update(dt, g_input);
                    animationPage.draw(canvas);
                    break;
                case Page::Math: {
                    mathPage.update(dt, g_input, nav.contentArea());
                    MathCanvas mathCanvas(canvas, font);
                    mathPage.draw(mathCanvas, nav.contentArea(), contentScaleFactor);
                    break;
                }
            }

            g_wantHand = nav.hoversAnyTab(g_input) ||
                (currentPage == Page::Widgets && widgetsPage.hoversAnyWidget(g_input));

            renderer.draw(curves, /*overlay_rotation_deg=*/0, {}, {}, msdfQuads, shapeVerts);
        }
    } catch (const std::exception& e) {
        MessageBoxA(hwnd, e.what(), "windows_ui_demo fatal error", MB_ICONERROR);
        return 1;
    }
    return 0;
}
