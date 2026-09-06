# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`ui_demo` — a showcase app built **on top of the `vk_canvas` engine**
(`libs/firstparty/vk_canvas`) and **`app_shell`**
(`libs/firstparty/app_shell`), running on **Linux/Wayland and Android from one
set of sources**. One window, a scrolling top nav, ten pages: text, shapes and
curves, widgets, animation, math, HDR output, images and tone mapping, gestures
and a swipe pager, text entry with undo/redo and IME, and a plot view.
Grayscale look throughout — app policy via `src/app/gray.h`'s equal-channel
`Color` helper, NOT an engine mode (see the vk_canvas CLAUDE.md's "Color
policy").

This repo owns **only the pages and their glue** under `src/`. Everything else
— Vulkan bootstrap, swapchain, SDF shapes, MSDF text, per-frame input edges,
layout, animation, gestures, pager, plot view, text buffer and undo stack —
comes from the library. The window, the message pump, the soft keyboard, the
safe-area insets and both entry points come from `app_shell`.

## The port, and what it costs to forget

This was a Win32-only app. `main.cpp` held a `wWinMain`, a `wnd_proc`, a
`PeekMessage` loop and the frame body in one file, and the five pages were
already portable. The port replaced that file with:

```
src/app/demo_app.{hh,cc}   DemoApp : FrameInputView : AppView — the application
src/app/gui_main.cc        app_shell_main()  — the Wayland entry point
platform/android/src/main.cc  android_main() — the Android entry point
```

`DemoApp` derives `FrameInputView`, app_shell's adapter that turns AppView
callbacks into the `FrameInput` the pages already read. **Nothing under `src/`
names an OS.** If a page needs something only a platform can answer, it takes a
callback and the app supplies it — see `TextEditPage::setKeyboardHooks` and
`setGlyphHook`. Adding an `#ifdef _WIN32` or an Android type to `src/` is the
thing this whole structure exists to prevent.

## Build

```
platform/linux/build.sh [debug|release|clean]     # -> build/linux_debug/ui_demo
cd platform/android && ./gradlew assembleDebug    # -> app/build/outputs/apk/
```

**Run the Linux build before the Android one.** `atlas_gen` is a HOST tool and
cannot run cross-compiled, so `platform/android/app/build.gradle` copies the
baked atlas out of `build/linux_debug/generated_fonts/math`. It fails with a
message saying so rather than shipping an APK whose font is missing — which
would fail at runtime with nothing on screen to explain it.

`slangc` is at `/opt/shader-slang/bin/slangc`; there is **no Vulkan SDK** on
this machine, so both build paths pass `-DVCE_SLANGC` explicitly.

`UI_DEMO_PAGE=<name>` opens a chosen page (`hdr`, `plot`, `textedit`, …). It
exists because rendering is verified by looking at it, and every page but the
first was otherwise unreachable from a script.

### Three ordering hazards, all of which have bitten

1. **`git submodule update` rewinds an unstaged pin.** `platform/linux/build.sh`
   runs it. Stage a gitlink you moved on purpose *before* building.
2. **AGP merges assets before the native build runs.** `platform/android/CMakeLists.txt`
   writes the compiled shaders straight into `app/src/main/assets/shaders`, and
   AGP cannot know that write is coming — it only tracks declared source-set
   inputs. `app/build.gradle` makes `merge*Assets` depend on `buildCMake*` for
   exactly this reason. Without it the APK builds, installs, launches, and has
   no shaders.
3. **Never list `assets/` as an Android `assets.srcDirs`.** `assets/fonts` is
   the shared `fonts` submodule and it is 127 MiB; this app opens three faces
   plus the atlas. The Gradle build stages exactly those into a generated
   directory and packages that.

### Font atlas

`atlas_gen` bakes NewComputerModern Roman/Bold/Italic + the Math face (glyphs
AND OpenType MATH metrics) into one MTSDF atlas, loaded at startup in
milliseconds. It is built as an isolated nested CMake project because it
vendors its own FreeType/msdfgen copies that would collide with
`vk_font_core`'s in one configure pass.

**CJK is baked at startup**, from the committed fallback faces, into the same
atlas before `initMsdf()` uploads it. **Batch all missing codepoints into ONE
`bakeCodepoints` call per fallback font** (`bakeMissing` in `demo_app.cc`):
each call appends whole atlas rows, and the offline atlas leaves only ~800 px
of headroom under the engine's 4096 px cap — per-codepoint calls burn a ~100 px
row per glyph and start failing after ~8 glyphs. A real bug, hit here once.

**Anything typed after startup needs `DemoApp::ensureGlyphs()`.** A glyph the
atlas lacks renders as *nothing* — the row goes blank rather than wrong, which
is the hard kind to notice. `initMsdf()` is built to be called again (it
patches only the pages that changed), so the fix is bake-and-re-upload, and the
IME path in `page_textedit.cc` does it on every whole-buffer edit.

## HDR

`DemoApp::create()` asks for `OutputTarget::Hdr10PQ` and then asks
`Renderer::hdrActive()` what it actually got. **Never assume the request was
granted** — the driver, the compositor, or a window that was never put into HDR
colour mode can each refuse it silently, and a fallback to the SDR pin looks
exactly like success until you notice nothing is brighter than white. On
Android the request is the `io.nava.appshell.HDR` manifest meta-data, which
`AppShellActivity` reads in `onCreate` *before* the surface exists, because the
colour mode changes which `VkSurfaceFormatKHR` pairs the driver enumerates.

Two things about drawing under it, both non-obvious:

- **UI colours are clamped to graphics white and cannot show headroom.**
  `output_encode.slang`'s `encodeUiColor()` saturates, deliberately — "UI never
  blazes". So shapes and text look identical on an HDR and an SDR swapchain. The
  HDR ramp and the tone-mapping picture are float textures through
  `Canvas::image` for this reason; a shape ramp would have been a lie.
- **Use `imageFg`, not `image`.** `Renderer::draw` records background images,
  *then* shapes, then foreground images, then text. This app paints a
  full-screen mid-gray rect as its page background — a shape — so a
  background-layer image is invisible underneath it no matter how correct the
  texture is.

## Architecture

```
src/app/
  demo_app.{hh,cc}  DemoApp: renderer + font + pages + the frame loop
  gui_main.cc       app_shell_main() — Wayland
  nav.h             TopNav: ten tabs, width-clamped scale with a floor, and
                    horizontal scrolling past it (ten tabs do not fit a phone)
  gray.h            gray(v, a) -> equal-channel Color (the whole policy)
  keys.h            aliases into vk_canvas's portable key:: space
  widgets_gray.h    this demo's stateful Button/Toggle/Slider/ListBox. Layout
                    mutates x/y/w/h in place; reconstructing a widget resets its
                    private click tracking and kills clicks (a bug hit here once)
  page_*.{h,cc}     the ten pages
src/math/           mathcore <-> vk_canvas adapters
```

**Per-frame sequence** (`DemoApp::draw`): `beginFrame()` → `host_->pump()` →
build a `Canvas` with `useMsdf` / `useShapes` / `useImagesFg` → full-screen
mid-gray rect over the WHOLE surface (insets included — letting the black clear
show under a notch reads as a bug) → `UiScale` factors from the **safe area**,
not the window → nav → the current page → `renderer_->draw(...)`.

`curves` stays empty every frame: everything rides the SDF shape path, so the
compute rasterizer's screen-sized buffers are never allocated and frames fully
overlap.

**Layout** comes from the safe area (`Host::safeInsets()`), because a phone's
camera cutout is glass and cannot be drawn under. System bars are software and
are hidden instead, so they are never insets.

## Tests

No unit tests of its own — the pure-logic modules that had them live in
vk_canvas now, with their tests. `libs/mathcore` builds its own test exes via
this same build. Rendering is verified by running the app and looking at it
(`UI_DEMO_PAGE=…` plus a screenshot).

## Committing

Use `./git_wrapper` (or the workspace copy) rather than plain git — it forces
the `nava` identity, strips co-author trailers, and pushes submodules before
the parent. Both `libs/firstparty/*` submodules are pinned; advance a pin by
committing in that library first, then staging the gitlink here.

## Global constraints

- This app is a **consumer**: no Vulkan calls, no shaders, no platform message
  handling anywhere in `src/`. If something feels missing from the engine, add
  it to the library (and its tests), not here. That is how the five portable
  modules the pager and plot pages needed came to be compiled into
  `vk_canvas_core` rather than copied in here.
- Grayscale via `gray()` at every call site — never a colored `Color` literal
  in `src/`.
- `assets/fonts` is a shared submodule. Never copy it wholesale into a build
  output or an APK.
