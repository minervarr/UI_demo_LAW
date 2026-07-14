# windows_UI_demo

A standalone Vulkan grayscale UI showcase — text (MTSDF via `vulkan_font_engine`),
shapes/curves, interactive widgets, and animation. No audio content; see
`docs/superpowers/specs/2026-07-11-vulkan-bw-ui-demo-design.md` for the design
and `docs/superpowers/plans/2026-07-11-vulkan-bw-ui-demo.md` for how it was built.

## Build

Requires Visual Studio Build Tools (MSVC), CMake, Ninja, and the Vulkan SDK
(for `glslc`/`slangc`).

    build.bat

Output: `build\windows_ui_demo.exe` (or `build_debug\` for a Debug build).

## Layout

- `src/platform` — Win32 window + input
- `src/gfx` — Vulkan bootstrap + the grayscale primitive batcher/pipeline
- `src/text` — MTSDF text adapter over `libs/firstparty/vulkan_font_engine`
- `src/ui` — Button/Toggle/Slider/ListBox widgets
- `src/anim` — app-level animation (the renderer has no animation primitive)
- `src/app` — the four showcase pages behind a top nav
