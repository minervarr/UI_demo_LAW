# windows_UI_demo

A Win32 grayscale UI showcase built on the
[vk_canvas](https://github.com/minervarr/Vk_Canvas_Lb_LAW) engine (submodule) —
text (MTSDF, Regular/Bold/Italic + CJK fallback), shapes/curves, interactive
widgets, animation, and live math typesetting/evaluation via the bundled
`libs/mathcore` library. The app itself is only the five pages plus glue; the
whole rendering/input/layout stack comes from the library.

## Setup

    git submodule update --init --recursive

## Build

Requires Visual Studio Build Tools (MSVC), CMake, Ninja, and the Vulkan SDK
(for `slangc`).

    platform\windows\build.bat

Output: `build\windows_ui_demo.exe` (or `build_debug\` for a Debug build).

## Layout

- `src/app` — the five showcase pages behind a top nav, plus `main.cpp`'s
  window/frame loop and this demo's grayscale widget set
- `src/math` — mathcore↔vk_canvas adapters (`IMathCanvas` over `Canvas`,
  `IMathFontMetrics` over the engine's `MsdfFont`)
- `libs/firstparty/vk_canvas` — the engine (submodule; brings its own
  `vulkan_font_engine` + `img_decode_kit`)
- `libs/mathcore` — standalone math lexer/parser/CAS/editor/typesetting
- `fonts/` — NewComputerModern (atlas source) + CJK fallback fonts
