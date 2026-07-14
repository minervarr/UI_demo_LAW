# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A standalone Win32 + Vulkan grayscale UI showcase (`windows_ui_demo`) demonstrating text (MTSDF), shapes/curves, interactive widgets, and animation in one window with a top nav across four pages. Zero audio content, zero chroma — every fragment shader writes only `R == G == B`.

## Build

Requires Visual Studio Build Tools (MSVC), CMake, Ninja, and the Vulkan SDK (for `glslc`/`slangc`).

```
build.bat            # interactive: prompts Release/Debug
build.bat debug       # build_debug\windows_ui_demo.exe
build.bat release     # build\windows_ui_demo.exe
build.bat clean debug # wipe build_debug\ first
```

`build.bat` runs `vcvars64.bat` before invoking CMake/Ninja — **never invoke `cmake`/`ninja` directly** outside that environment, or MSVC headers (`windows.h`, etc.) won't resolve. It also runs `git submodule update --init --recursive` before configuring.

Building via `build.bat` also compiles shaders (`glslc` for the app's own GLSL shaders, `slangc` for the vendored MSDF Slang shaders) and copies `fonts/` next to the exe as a post-build step.

## Tests

Five unit-test executables, each built via the same `build.bat`/CMake flow (or directly: `cmake --build build_debug --target <name>`), then run as a normal exe (`.\build_debug\<name>.exe`) — plain `assert()`-based, no framework:

- `input_state_test` — mouse edge-detection (`mouseWentDown`/`mouseWentUp`) semantics
- `primitives_test` — `PrimitiveBatch` vertex math (rect/rounded-rect/line/bezier geometry, capsule-SDF fields)
- `animated_float_test` — easing/interpolation math
- `widgets_test` — hit-testing and value-mapping for Button/Toggle/Slider/ListBox
- `layout_test` — `UiScale`/dock/`RowCursor`/`ColumnCursor` layout math

Rendering itself has **no automated test suite** — verified by running the app and checking visually. Only pure-logic pieces (the five above) get real unit tests.

## Architecture

**Render pass ownership split across two independent pipelines sharing one `VkRenderPass`:**
- `gfx/vk_core` — Vulkan bootstrap (instance/device/swapchain/render pass/sync objects). Produces `FrameContext{cmd, renderPass, extent}` each frame via `beginFrame()`/`endFrame()`. Clears to mid-gray. Fail-fast (`MessageBoxA` + `ExitProcess`) on any Vulkan init failure — no retry/fallback logic anywhere in this codebase. Validation layers are Debug-only (`#ifdef _DEBUG`).
- `gfx/primitives` + `gfx/shape_pipeline` — a single CPU-side triangle batcher (`PrimitiveBatch`) feeding one grayscale pipeline (`shaders/shape.vert`/`.frag`, compiled via `glslc`). Rects and rounded-rect corners are flat hard-edged triangles. **Lines and bezier curves are anti-aliased analytically**: each segment is emitted as an oriented, margin-expanded quad carrying its true capsule endpoints (`ax,ay,bx,by`) and `radius` per-vertex; the fragment shader computes a signed distance to that capsule and converts it to `fwidth`-normalized coverage (same technique as the sibling repo `windows_matrix_player`'s `vk_canvas`, no MSAA). Non-capsule (flat) vertices carry `radius = -1.0f` as a sentinel to skip the SDF branch. `Vertex` in `primitives.h` documents the full field layout.
- `text/text_renderer` — a thin adapter over the vendored `vulkan_font_engine` submodule's MTSDF text stack (`MsdfFont` + `MsdfTextRenderer`, own pipeline, own render pass usage against the same `VkRenderPass`). Renders three real baked faces — Regular (weight 0), Bold (weight 1), Italic (weight 2) — resolved via `MsdfFont::keyForStyle()` + `layoutByKey()`, **not** a synthesized thickened/sheared glyph. There is no combined Bold+Italic face (the vendored engine's `FontStyle` enum has no such slot); if both are requested, Bold wins. Initialization is split into two phases to avoid blocking the UI thread: `bakeFonts()` (CPU-only msdfgen/FreeType rasterization + disk cache, safe to run on a background `std::thread`) and `finishGpuInit()` (Vulkan resource creation, main-thread-only, gated on the `fontsBaked()` atomic flag with acquire/release ordering). `main.cpp` spawns the bake thread at startup and must join it on every exit path before destroying `text`/`vk`.

**`anim/animated_float`** — app-level interpolation (`AnimatedFloat` + `easeLinear`/`easeInOutCubic`). The renderer itself has no animation primitive; pages own their own `AnimatedFloat` instances and call `update(dtSeconds)` each frame.

**`ui/widgets`** — `Button`/`Toggle`/`Slider`/`ListBox`, each with `update(const InputState&)` (hit-testing/interaction, returns whether a completed interaction happened that frame) and `draw(...)` (geometry via `PrimitiveBatch`, labels via `TextRenderer`). All have explicit constructors (not aggregates — each has a private tracking field) so they can't be default-brace-initialized; layout code mutates their `x/y/w/h` fields in place across frames rather than reconstructing them (reconstructing would silently reset private click-tracking state and make the widget permanently unresponsive — a real bug that was caught and fixed once already).

**`ui/layout`** — `Rect`, `UiScale` (proportional scale: `max(actualHeight/referenceHeight, floorScale)`, uncapped above, floored below — never negative/zero), `dockTop/Bottom/Left/Right` (mutate a container rect in place, return the docked strip), `centerIn`, `RowCursor`/`ColumnCursor` (gap-chaining). Every page computes its layout from the current window size each frame via this module — **no hardcoded absolute pixel positions** anywhere in `src/app`. This mirrors the resolution-robust technique validated in the sibling repo `windows_matrix_player` (`ResponsiveTextScale` + `PlayerWindow::recalcLayout()`), reimplemented standalone since this repo has no dependency on `vk_canvas`.

**`app/`** — `nav.h` (`Page` enum + `TopNav`), four pages (`page_text`, `page_shapes`, `page_widgets`, `page_animation`), and `main.cpp`'s `wWinMain` which owns the per-frame sequence: pump messages → resize → `beginFrame` → one-time `shapes`/`text` init (gated by `shapesReady`/`textReady` bools, not both on the same frame) → compute `Rect windowRect`/`uiScaleFactor` → `nav.updateLayout`/`update`/`draw` → dispatch to the current page's `updateLayout`/`update`/`draw` → `shapes.draw`/`text.draw` → `endFrame`.

**`platform/`** — `Window` (Win32 message pump, `WM_SIZE`/`WM_LBUTTONDOWN`/`WM_LBUTTONUP`/`WM_MOUSEMOVE` → `InputState`), `InputState` (edge-detected mouse state: `mouseDown` persists, `mouseWentDown`/`mouseWentUp` are true only on the transition frame — call `beginFrame()` once per frame before pumping messages), `paths::exeDirectory()` (resolves shader/font paths relative to the running exe, not the process's CWD — required so the app works regardless of launch directory).

## Global constraints (apply repo-wide)

- Zero audio_engine content — this is a from-scratch UI demo, not derived from any audio-app code.
- Only git submodule: `libs/firstparty/vulkan_font_engine`. No `vk_canvas`, no `libusb`.
- Grayscale-by-construction: every fragment shader's only output channels that vary are alpha/coverage — R, G, B are always equal.
- Build exclusively via `build.bat` (CMake + Ninja + MSVC) — no `.sln` files.
- Fail-fast, no retry/fallback logic on Vulkan/font-init failure (message box + exit).
