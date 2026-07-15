# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`windows_ui_demo` — a Win32 showcase app built **on top of the `vk_canvas` engine** (`libs/firstparty/vk_canvas`, the [Vk_Canvas_Lb_LAW](https://github.com/minervarr/Vk_Canvas_Lb_LAW) submodule). One window, a top nav, five pages: text (MTSDF, Regular/Bold/Italic + CJK fallback), shapes/curves, interactive widgets, animation, and live math typesetting/evaluation (via `libs/mathcore`). Grayscale look throughout — app policy via `src/app/gray.h`'s equal-channel `Color` helper, NOT an engine mode (see the vk_canvas CLAUDE.md's "Color policy").

This repo owns **only the pages and their glue** (~10 small files under `src/`). Everything else — Vulkan bootstrap, swapchain, SDF shape pipeline, MSDF text, per-frame input edges, layout math, animation primitive, Win32 platform seams — comes from the library. A previous iteration of this repo reimplemented all of that from scratch; that code was deleted once the reusable parts were ported into vk_canvas (core `layout.hh` / `animated_float.hh` / `frame_input.hh` + unit tests + the `CharEvent`/`onChar` text-entry seam all originated here).

## Build

Requires Visual Studio Build Tools (MSVC), CMake, Ninja, and the Vulkan SDK (for `slangc`).

```
platform\windows\build.bat            # interactive: prompts Release/Debug
platform\windows\build.bat debug       # build_debug\windows_ui_demo.exe
platform\windows\build.bat release     # build\windows_ui_demo.exe
platform\windows\build.bat clean debug # wipe build_debug\ first
```

`build.bat` runs `vcvars64.bat` before invoking CMake/Ninja — **never invoke `cmake`/`ninja` directly** outside that environment, or MSVC headers won't resolve. It also runs `git submodule update --init --recursive` before configuring — which means **an un-staged submodule advance gets rewound by the build**; stage the new gitlink (`git add libs/firstparty/vk_canvas`) before building if you intentionally moved the pin.

Building also compiles all shaders from the library's sources (`vce_compile_slang`, including `msdf_vert/frag` — required because this app calls `Renderer::initMsdf`; the library's own demo doesn't compile them), bakes the font atlas (below), and copies `fonts/` + the baked atlas into `<build>/assets/` next to the exe (the library's `FileAssetReader` reads `<exe dir>/assets/`).

### Font atlas (generated at build time, not committed)

`atlas_gen` (a host tool vendored in the font engine, built as an isolated nested CMake project because it vendors its own FreeType/msdfgen copies that would collide with `vk_font_core`'s in one configure pass) bakes NewComputerModern **Roman/Bold/Italic + the Math face — glyphs AND OpenType MATH metrics — into one MTSDF atlas** (`font.msdf` + `atlas.rgba`, 3072px wide). The app loads this single `MsdfFont` at startup in milliseconds; there is **no runtime bake thread** (the old two-phase background bake existed only because the previous architecture rasterized fonts at runtime).

**CJK fallback**: the offline atlas covers ASCII/Latin-1 + math; the three CJK showcase strings are baked at startup from the committed fallback fonts (`fonts/fandol`, `fonts/haranoaji`, `fonts/unfonts-core`) via `MsdfFont::bakeCodepoints()`, appended to the same atlas before `initMsdf()` uploads it. **Batch all missing codepoints into ONE `bakeCodepoints` call per fallback font** (see `bakeCjkFallbacks` in main.cpp): each call appends whole atlas rows, and the offline atlas leaves only ~800px of headroom under the engine's 4096px cap — per-codepoint calls burn a ~100px row per glyph and start failing after ~8 glyphs (a real bug hit here once).

## Architecture

```
src/app/
  main.cpp          wWinMain: window + wnd_proc (win32_translate_input -> FrameInput,
                    WM_SETCURSOR hand/arrow), Renderer + MsdfFont startup, per-frame loop
  nav.h             TopNav: 5 tabs (Button), width-clamped scale, dockTop content area
  gray.h            gray(v, a) -> equal-channel Color (the whole grayscale policy)
  keys.h            VK_* numeric constants for FrameInput::keysWentDown
  widgets_gray.h    this demo's stateful Button/Toggle/Slider/ListBox over Canvas +
                    FrameInput (NOT the library's stateless widgets:: — different look).
                    Layout mutates x/y/w/h in place; reconstructing a widget resets its
                    private click tracking and kills clicks (bug hit + fixed here once)
  page_text.*       styled sizes ladder + Bold/Italic + CJK, wheel-scrolled + clipped
  page_shapes.*     rect / rounded rect / segment / bezier-as-polyline / gray ramp
  page_widgets.*    the widgets_gray set wired to FrameInput
  page_animation.*  AnimatedFloat fade+slide square, resize re-anchoring
  page_math.*       4 static CachedExpressions + live editable expression (typedChars
                    + backspace/arrows), wheel-scrolled + clipped
src/math/
  math_canvas.h                MathCanvas: mathcore::IMathCanvas over Canvas (baseline->top
                               y conversion in text(); clip forwards to Canvas)
  msdf_font_metrics_adapter.*  mathcore::IMathFontMetrics over the engine's MsdfFont
libs/
  firstparty/vk_canvas   submodule: the engine (contains vulkan_font_engine + img_decode_kit)
  mathcore               standalone math library (lexer/parser/CAS/editor/typesetting), own tests
```

**Per-frame sequence** (main.cpp): `g_input.beginFrame()` → pump messages (fills `FrameInput` via the library's `win32_translate_input`; `TranslateMessage` generates the WM_CHAR the math editor reads) → clear vectors → construct `Canvas` with `useMsdf(&font, &msdfQuads)` + `useShapes(&shapeVerts)` → full-screen mid-gray rect (the render pass clears to black) → `UiScale{661, 0.5}` factors from the current window height (capped at 1.6 for the vertically-stacking Text/Math pages) → nav update/draw → current page update/draw → `renderer.draw(curves /*empty*/, 0, {}, {}, msdfQuads, shapeVerts)`.

The `curves` vector stays empty every frame: all primitives ride the library's SDF shape quad path, so the compute rasterizer's screen-size buffers are never allocated and frames fully overlap (see the library's "Frames in flight" notes).

**Layout**: every page computes its layout from the current window size each frame via the library's `layout.hh` (`UiScale`/dock/cursors) — no hardcoded absolute pixel positions in `src/`.

**Text semantics**: `Canvas::text()` takes the text-box TOP (`baseline = y + size`); the old renderer took a baseline directly. All ported baseline math is converted at the call sites (grep for "baseline" comments). `MathCanvas::text()` still takes a baseline (mathcore's contract) and converts internally.

## Tests

This repo has no unit tests of its own anymore — the pure-logic modules that had them (layout, animated_float, input edges) live in vk_canvas now, with their tests (`core/tests/` there). `libs/mathcore` builds its own test exes (`parser_test`, `evaluator_test`, `editor_test`, ...) via this same build; run them from the build folder. Rendering is verified by running the app and checking visually.

## Committing

Use the vk_canvas repo's `git_wrapper.exe` (`commit`/`push`/`save`) rather than plain git — it forces the `nava` identity, strips co-author trailers, and pushes submodules before the parent. The `vk_canvas` submodule is pinned/detached, so the wrapper skips it on push (advance its pin by committing in the library repo first, then updating the gitlink here).

## Global constraints (apply repo-wide)

- This app is a **consumer** of vk_canvas: no Vulkan calls, no shaders, no platform message handling outside `main.cpp`'s thin wnd_proc. If something feels missing from the engine, add it to the library (and its tests), not here.
- Grayscale look via `gray()` at every call site — never a colored `Color` literal in `src/`.
- Only submodule: `libs/firstparty/vk_canvas`. `libs/mathcore` is committed source.
- Build exclusively via `platform\windows\build.bat` — no `.sln` files.
