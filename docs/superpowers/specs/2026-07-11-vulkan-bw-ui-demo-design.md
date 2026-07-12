# windows_UI_demo — Vulkan Grayscale UI Showcase (Design)

## Purpose

A standalone template/demo app proving out a brand-new, minimal Vulkan renderer built
around pure grayscale rendering. This is **not** an audio_engine demo — no audio_engine
code, headers, or concepts are referenced anywhere in this project. It exists purely to
showcase what the new renderer can do so it can later be lifted into `vk_canvas` as an
alternate lightweight backend (that upstreaming is a separate future task, out of scope
here).

## Repository

`C:\Users\incxiuefb\Documents\Files\clone\windows_UI_demo`
(remote: `https://github.com/minervarr/win_UI_demo.git`, already initialized with one commit)

## Submodules

- `libs/firstparty/vulkan_font_engine` → `https://github.com/minervarr/vulkan_font_engine`
  (MTSDF text rendering — same submodule vk_canvas uses internally, taken directly rather
  than pulling in all of vk_canvas)

No other firstparty/thirdparty dependencies. In particular: **no audio_engine, no libusb,
no dr_flac, no sqlite3** — this project has zero audio content.

## Directory layout

```
windows_UI_demo/
  libs/firstparty/vulkan_font_engine/   — submodule, MTSDF text
  src/
    platform/   — Win32 window creation, message pump, mouse/keyboard input state
    gfx/        — new minimal Vulkan renderer
                    - core: instance/device/swapchain/frame sync, one grayscale-output
                      pipeline for quads/lines/beziers
                    - primitives: batcher API (pushRect, pushRoundedRect, pushLine,
                      pushBezier, pushGlyphQuad) — one draw call per pipeline per frame
    text/       — thin adapter over vulkan_font_engine: layout + MTSDF atlas upload/sample,
                   feeds glyph quads into gfx/primitives
    ui/         — immediate-mode-style widgets (Button, Slider, Toggle, List) built from
                   gfx primitives + text; hover/press states are brightness/opacity deltas
                   only, never color
    anim/       — `Animated<T>` value (duration + easing curve), ticked once per frame —
                   the renderer itself has no animation primitive, so interpolation lives
                   at the app layer
    app/        — WinMain + the four showcase pages behind a simple top nav
  build.bat
  CMakeLists.txt
  docs/superpowers/specs/   — this file
```

## Showcase pages

A single window with a top nav switching between four pages:

1. **Text** — vulkan_font_engine MTSDF glyphs at multiple sizes/weights, crisp at both
   small and large scale.
2. **Shapes & Curves** — rects, rounded panels, lines, bezier curves exercising the
   quad/line pipeline.
3. **Interactive Widgets** — Button, Slider, Toggle, List responding to real mouse input
   (hover/press/drag), proving the renderer drives interaction, not just static frames.
4. **Animation** — continuous motion, fades, and eased transitions driven by `anim/`,
   proving the render loop sustains frame rate under continuous redraw.

## Grayscale-by-construction

Windows' compositor requires the swapchain to present in a standard multi-channel format
(BGRA8/RGBA8) — there is no true single-channel presentation format. "Pure black and
white" is therefore enforced by discipline rather than by the swapchain format itself:

- Every fragment shader writes only `R == G == B` — no chroma value is ever produced,
  anywhere in the pipeline.
- All offscreen/procedural textures (masks, gradients, anything not presented directly)
  use `R8_UNORM` instead of RGBA — roughly a 4x memory cut versus a full-color pipeline.
- The one exception is the MTSDF font atlas, which inherently needs multi-channel
  distance-field encoding to reconstruct sharp glyph edges — its *output*, once sampled
  and drawn, is still constrained to grayscale by the same fragment-shader discipline.

## Data flow

Continuous redraw loop (not WM_PAINT-driven — the Animation page needs steady frame
pacing regardless of input):

```
Win32 message pump → input state (mouse/keys)
  → app/ updates current page's widget + animation state
  → gfx/primitives batch built for the frame
  → gfx/core records one command buffer, submits, presents
```

## Error handling

Fail-fast: any Vulkan initialization failure shows a message box and exits. Validation
layers are enabled in Debug builds only. This is a demo/template, not production code —
no retry, fallback, or recovery logic is implemented anywhere.

## Testing

No automated test suite. This is a visual renderer demo; correctness is verified by
running the app and visually checking each of the four pages. This is a deliberate
choice, not an oversight — there is no meaningful unit-testable logic here beyond the
`anim/` easing math, which is simple enough not to warrant its own test harness.

## Build

Same pattern as `windows_matrix_player`: CMake + Ninja + MSVC via `build.bat`
(`vcvars64.bat` → cmake configure → ninja build). No .sln files.

## Explicitly out of scope

- Any audio_engine integration, real or mocked.
- Upstreaming `gfx/` into `vk_canvas` as an alternate backend — stated future intent,
  tracked as a separate task once this renderer is proven out here.
- True 1-bit/dithered rendering — grayscale (many shades) was chosen over strict
  black-or-white pixels.
