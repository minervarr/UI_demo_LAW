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

### Fonts: two paths, and why both are here

**`uiFont_` is a `RasterFont`** and serves nine of the ten pages. Faces are
opened straight from the shared `fonts` submodule at runtime — `open()`,
`addStyle()` for Bold/Italic, `addFallback()` per style for CJK — and glyphs are
rasterized lazily per size. No offline bake, no atlas cache, nothing to keep in
step. This is the path Matrix Player uses and the one to reach for.

Fallback chains are registered **per style**. A bold CJK label whose Bold chain
is missing silently resolves back to the Regular face: it still renders, at the
wrong weight, which looks fine until it sits beside bold Latin. Chain order is
Chinese → Japanese → Korean and it matters — all three cover Han and Kana, only
the Korean face has Hangul.

The faces are the **serif** cuts (Song / Mincho / Batang), matched to New
Computer Modern's serif Latin. The sans and calligraphic cuts bundled beside
them are deliberately not registered: whichever face won a codepoint would
decide the look, and a line would come out in mixed styles.

`buildUiFont()` seeds printable ASCII at the common sizes. That is not an
optimisation — an atlas with nothing in it has no pixels, and binding it makes
`initMsdf()` log "atlas pixels not resident" and decline to build the pipeline.
Every other size arrives through **misses**: `layout()` records what it was
asked for and could not draw, `drainGlyphMisses()` bakes them after the frame.
One frame of a missing glyph the first time a size appears, never again.

**`mathFont_` is an `MsdfFont`** loaded from `atlas_gen`'s offline MTSDF bake,
and it exists for exactly one page. OpenType MATH tables — `MathConstants`, the
variant/assembly constructions, `buildVStretch()` — are produced ONLY by that
offline bake (`msdf.cc` sets `hasMath_` inside `load()` and nowhere else), and
the `TextFont` seam every other font goes through has no notion of math at all.
Matrix Player gets to be pure `RasterFont` because it never renders math; this
demo does. **Do not "simplify" this away** — deleting the MTSDF path deletes
math typesetting with it.

The Renderer holds ONE atlas, so the two are **swapped on page change** by
`bindFont()`, not mixed. That is the expensive branch of `initMsdf()` (it waits
for in-flight frames and rebuilds), which is affordable once per page change and
would not be per frame.

Because the math atlas is a host-tool product, the Android build copies it out
of the desktop build tree — which is why `platform/linux/build.sh` must run
first. Only the math page needs it; the other nine would build without it, and
the app starts with `mathAvailable_ = false` rather than refusing to run.

## HDR

**Three different facts, and conflating them is the bug this page was reported
for:**

1. **did we ask?** — `hdrRequested_`
2. **what did the swapchain become?** — `hdrActive()`
3. **what can the display actually do?** — measured headroom

The second does not imply the third. `pickTarget()` in vk_canvas only checks
whether the SURFACE advertises an HDR format/colorspace pair, and a wlroots
compositor advertises HDR10 ST 2084 whatever the monitor is. So asking
unconditionally made the app announce "HDR active" on a plain SDR display while
the compositor quietly tone-mapped the PQ back down — strictly worse than having
rendered SDR.

So: **the desktop does not request HDR by default.** Nothing available here can
verify the display (knowing a Wayland output's real capability needs the
colour-management protocol, which vk_canvas does not implement), and a request
we cannot verify is a claim we cannot make. `UI_DEMO_HDR=1` opts in. Android
does request it — the manifest meta-data app_shell reads — because there the
platform can be asked afterwards.

**Headroom is measured, never assumed.** On Android that is
`activity::display_hdr_headroom()`, which app_shell backs with
`Display.getHdrSdrRatio()` (live) or `HdrCapabilities` (static). Everywhere else
it is **1.0**, meaning none. This matters because headroom is what the
clip-warning stripes are drawn against: with a made-up 4.0, nothing between 1.0
and 4.0 was striped on a screen that could show none of it. The magenta stripes
mark what THIS display cannot reach, so the number behind them has to be real.

Two things about drawing under an HDR target, both non-obvious:

- **UI colours are clamped to graphics white and cannot show headroom.**
  `output_encode.slang`'s `encodeUiColor()` saturates, deliberately — "UI never
  blazes". Shapes and text therefore look identical on an HDR and an SDR
  swapchain. The HDR ramp and the tone-mapping picture are float textures
  through `Canvas::image` for exactly this reason; a ramp made of rects would
  have been a lie.
- **Use `imageFg`, not `image`.** `Renderer::draw` records background images,
  *then* shapes, then foreground images, then text. This app paints a
  full-screen rect as its page background, so a background-layer image is
  invisible underneath it no matter how correct the texture is.

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
