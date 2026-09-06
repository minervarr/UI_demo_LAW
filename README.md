# ui_demo

A grayscale UI showcase built on the
[vk_canvas](https://github.com/minervarr/Vk_Canvas_Lb_LAW) engine (submodule),
running on **Linux/Wayland and Android from the same sources**: text (MTSDF,
Regular/Bold/Italic + CJK fallback), shapes and curves, interactive widgets,
animation, live math typesetting and evaluation, HDR output, images and tone
mapping, touch gestures with a swipe pager, text entry with undo/redo and IME,
and a pan/zoom plot view.

The app owns only the pages and their glue. The whole rendering stack comes
from `vk_canvas`, and the window, the message pump and the platform seam come
from [`app_shell`](https://github.com/minervarr/App_shell) — which is what lets
one `DemoApp` be the application on both platforms.

## Setup

    git submodule update --init --recursive

## Build

**Linux (Wayland).** Needs CMake, Ninja, Vulkan, and `slangc`. There is no
Vulkan SDK on the development machine; slang is installed on its own, so the
script points at it explicitly.

    platform/linux/build.sh [debug|release|clean]

Output: `build/linux_debug/ui_demo`, with `assets/` (shaders, the baked font
atlas, the CJK fallback faces) beside it.

**Android.** Needs the SDK and NDK 29. Run the Linux build **first** — the
math page's atlas is baked by a host tool that cannot run cross-compiled, so
the APK takes it from the desktop build tree. Only that one page needs it.

    platform/linux/build.sh debug
    cd platform/android && ./gradlew assembleDebug

`platform/android/local.properties` (`sdk.dir=…`) is per-machine and
gitignored; write it once.

**Windows** is not currently a target. `app_shell` has a Win32 host and every
page is portable, so restoring it is build-system work rather than a rewrite —
but it is not carried here unbuilt and untested.

## HDR

The desktop does **not** request HDR by default. A compositor will hand out an
HDR10 swapchain whatever the monitor is, so a request that cannot be verified
would just make the app claim HDR it has no evidence for. `UI_DEMO_HDR=1` opts
in; Android requests it through the manifest, where the display can actually be
asked afterwards. The HDR page reports all three facts separately — what was
asked for, what the swapchain became, and what the display measures — and the
clip-warning stripes are drawn against the measured headroom, so magenta means
"this screen cannot show this".

## Small screens

Pages lay out at their natural size and scroll when the window cannot hold
them, on both axes, by drag as well as wheel — a touch screen has no wheel, and
that was the case the original wheel-only scrolling could not serve. Scrollbars
appear only when something is off-screen. Edge margins are 3 mm converted with
the display's real density rather than a pixel count, so the gap is the same
physical size on a phone and a monitor.

    build/linux_debug/scroll_area_test    # the scrolling and unit arithmetic

## Pages

`UI_DEMO_PAGE=<name>` opens the app on one page, which is how a screenshot of
any page but the first is taken without a human clicking a tab:
`text shapes widgets animation math hdr image gestures textedit plot`.

## Layout

- `src/app` — `DemoApp` (the `AppView`), the desktop entry point, the ten
  pages behind a scrolling top nav, and this demo's grayscale widget set
- `src/math` — mathcore↔vk_canvas adapters
- `platform/linux/build.sh`, `platform/android/` — the two platform entry
  points; `platform/android/src/main.cc` is the whole Android bootstrap
- `libs/firstparty/vk_canvas` — the engine (submodule)
- `libs/firstparty/app_shell` — the window/pump/Host seam (submodule)
- `libs/mathcore` — standalone math lexer/parser/CAS/editor/typesetting
- `assets/fonts` — the shared `fonts` submodule
