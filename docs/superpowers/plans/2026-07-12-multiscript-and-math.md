# Multi-Script Text + Math Typesetting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add UTF-8/multi-script text rendering with font fallback (Feature A, Tasks 1-3), and a new "Math" showcase page rendering real TeX-style typeset equations via a ported box-layout engine reading the vendored font engine's OpenType MATH table support (Feature B, Tasks 4-10).

**Architecture:** Feature A extends `TextRenderer` in place (UTF-8 decode + `bakeCodepoints`-based fallback, already-present engine primitives, no new files beyond page content). Feature B adds a second, independent `MsdfFont` instance loaded from an offline-baked MATH-table atlas (built once via the vendored engine's `atlas_gen` host tool, committed as a binary asset), a small `MathCanvas` adapter routing to existing `PrimitiveBatch`/`TextRenderer` primitives, and ported (not rewritten) parser + box-layout code from `C:\Users\incxiuefb\Documents\Files\clone\calculator` (same author, AGPLv3), simplified to this app's grayscale-only model (no `Color` type — every draw call takes a single `gray` float).

**Tech Stack:** C++17, Vulkan 1.3, the vendored `vulkan_font_engine` submodule (`libs/firstparty/vulkan_font_engine`), its host tool `tools/atlas_gen` (built once, not part of the app's own CMake build), same CMake+Ninja+MSVC toolchain as the rest of this repo.

## Global Constraints

- Grayscale-by-construction: every new draw call/type carries a single `gray` float, never RGB. The ported math code's `Color` type is dropped entirely — replace every `Color`-typed parameter/field with `float gray`, and drop the "negative-number tinting"/"cycled-paren-color" features from the ported code (not applicable, out of scope).
- Zero audio_engine content; only git submodule remains `libs/firstparty/vulkan_font_engine`.
- Build exclusively via `build.bat` (CMake + Ninja + MSVC) — never invoke `cmake`/`ninja` directly outside that environment, `windows.h` won't resolve.
- Commit via plain `git commit` (`git_wrapper.exe` is confirmed absent from PATH in this environment throughout this project) — never push.
- Fail-fast (message box + exit), no retry/fallback logic on Vulkan/font/asset load failure, matching the existing `fatal()` pattern in `src/gfx/vk_core.cpp` and `src/app/main.cpp`.
- No automated test suite for rendering — verified by running the app. Pure-logic code (the ported math parser/lexer) gets real unit tests, matching this repo's established convention (`primitives_test`, `widgets_test`, `layout_test`, etc.).
- Ported code license note: `core/math/*`, `mathbox.*`, `mathlayout.*` originate from `C:\Users\incxiuefb\Documents\Files\clone\calculator`, which is AGPLv3-licensed (same author). Carry this forward as a code comment at the top of each ported file — do not silently drop the provenance.

---

### Task 1: UTF-8 decoding in `TextRenderer::drawText`

**Files:**
- Modify: `src/text/text_renderer.h`
- Modify: `src/text/text_renderer.cpp`

**Interfaces:**
- Consumes: `utf8::nextCodepoint(std::string_view, size_t&)` from `libs/firstparty/vulkan_font_engine/core/utf8.hh` (already vendored, already used elsewhere in that submodule — `core/font.cc:289,297`, `core/msdf.cc:738`). Signature: returns the decoded `uint32_t` codepoint, advances the `size_t& i` index past the consumed bytes, returns U+FFFD (0xFFFD) and advances by 1 on malformed/truncated input (always makes forward progress).
- Produces: no interface change — `drawText`'s existing signature (`std::string_view text, float x, float baselineY, float sizePx, float gray, bool bold, bool italic`) is unchanged; only its internal iteration changes from bytes to codepoints.

- [ ] **Step 1: Add the include**

In `src/text/text_renderer.h`, add near the top (after the existing includes):
```cpp
#include "utf8.hh"
```

- [ ] **Step 2: Replace the Roman-path byte loop**

In `src/text/text_renderer.cpp`, find `TextRenderer::drawText`'s Roman early-return block (currently `for (char c : text) { penX = font_.emitGlyph(out, (uint32_t)(unsigned char)c, penX, baselineY, sizePx, gray, gray, gray, 1.0f); }` followed by `return;`). Replace it with:
```cpp
    if (style == FontStyle::Roman) {
        size_t i = 0;
        while (i < text.size()) {
            uint32_t cp = utf8::nextCodepoint(text, i);
            penX = font_.emitGlyph(out, cp, penX, baselineY, sizePx, gray, gray, gray, 1.0f);
        }
        return;
    }
```

- [ ] **Step 3: Replace the Bold/Italic-path byte loop**

Find the loop below it: `for (char c : text) { uint32_t cp = (uint32_t)(unsigned char)c; uint32_t key = font_.keyForStyle(style, cp); ... }`. Replace the outer loop and codepoint extraction with:
```cpp
    size_t i = 0;
    while (i < text.size()) {
        uint32_t cp = utf8::nextCodepoint(text, i);
        uint32_t key = font_.keyForStyle(style, cp);
        if (key == 0) {
            // Not covered by this style's baked face — fall back to the
            // Roman glyph rather than silently dropping the character.
            penX = font_.emitGlyph(out, cp, penX, baselineY, sizePx, gray, gray, gray, 1.0f);
            continue;
        }
        GlyphQuad q;
        penX = font_.layoutByKey(key, penX, baselineY, sizePx, q);
        if (!q.draw) continue;
        vert(q.x0, q.y0, q.u0, q.v0); vert(q.x1, q.y0, q.u1, q.v0); vert(q.x1, q.y1, q.u1, q.v1);
        vert(q.x0, q.y0, q.u0, q.v0); vert(q.x1, q.y1, q.u1, q.v1); vert(q.x0, q.y1, q.u0, q.v1);
    }
```
(keep the existing `vert` lambda above this block unchanged).

- [ ] **Step 4: Build and manually verify no regression**

```powershell
.\build.bat debug
```
Launch `build_debug\windows_ui_demo.exe`, navigate to the Text page, confirm all existing lines (regular/bold/italic, all ASCII) still render identically to before — this task only changes iteration granularity for ASCII input, which is byte-identical to codepoint-identical for single-byte UTF-8, so there should be zero visual change yet.

- [ ] **Step 5: Commit**

```bash
git add src/text/text_renderer.h src/text/text_renderer.cpp
git commit -m "Decode drawText input as UTF-8 codepoints instead of raw bytes"
```

---

### Task 2: Font-fallback chain (`ensureCodepointsBaked`)

**Files:**
- Modify: `src/text/text_renderer.h`
- Modify: `src/text/text_renderer.cpp`

**Interfaces:**
- Consumes: `MsdfFont::hasCodepoint(uint32_t) const` (`msdf.hh:183`), `MsdfFont::bakeCodepoints(AssetReader&, const char*, const std::vector<uint32_t>&)` returning `int` (count newly baked) (`msdf.hh:141`), `utf8::nextCodepoint` (Task 1).
- Produces: `void TextRenderer::ensureCodepointsBaked(std::string_view text)` — public method, safe to call from the same thread/context as `bakeFonts()` (CPU-only, no Vulkan calls). Also produces the new `bakeFonts(std::vector<std::string> extraTexts)` signature (was `bakeFonts()` with no params) that Task 3 and `main.cpp` depend on.

- [ ] **Step 1: Add `ensureCodepointsBaked` declaration**

In `src/text/text_renderer.h`, add to the public section (after `bakeSucceeded()`):
```cpp
    // Scans `text`'s UTF-8 codepoints; for any not already covered by the
    // primary Latin Modern face, tries a fixed ordered fallback font list
    // (CJK/Korean) via MsdfFont::bakeCodepoints(), which appends only the
    // missing glyphs to the shared default-face table (no new atlas/style
    // slot). CPU-only (msdfgen), same threading contract as bakeFonts() —
    // call before fontsBaked() is observed true by the main thread, or
    // accept a bake stall if called later for genuinely new text.
    void ensureCodepointsBaked(std::string_view text);
```

Change the `bakeFonts()` declaration to:
```cpp
    // extraTexts: additional strings (e.g. non-Latin showcase lines) whose
    // codepoints get baked via ensureCodepointsBaked() as part of this same
    // background-thread phase, so no additional bake stall happens later.
    bool bakeFonts(std::vector<std::string> extraTexts = {});
```

- [ ] **Step 2: Implement `ensureCodepointsBaked`**

In `src/text/text_renderer.cpp`, add before `TextRenderer::bakeFonts`:
```cpp
void TextRenderer::ensureCodepointsBaked(std::string_view text) {
    static const char* kFallbackFonts[] = {
        "fandol\\FandolHei-Regular.otf",
        "haranoaji\\HaranoAjiGothic-Regular.otf",
        "unfonts-core\\UnDotum.ttf",
    };
    std::string exeDir = exeDirectory();
    size_t i = 0;
    while (i < text.size()) {
        uint32_t cp = utf8::nextCodepoint(text, i);
        if (font_.hasCodepoint(cp)) continue;
        for (const char* rel : kFallbackFonts) {
            std::string path = exeDir + "\\fonts\\" + rel;
            if (font_.bakeCodepoints(assets_, path.c_str(), {cp}) > 0) break;
        }
    }
}
```

- [ ] **Step 3: Wire `extraTexts` into `bakeFonts`**

In `TextRenderer::bakeFonts`, change the signature to `bool TextRenderer::bakeFonts(std::vector<std::string> extraTexts)` and, right before the existing `fontsBaked_.store(true, ...)` line (after the Italic `addStyle` block, still inside `if (ok)`), add:
```cpp
        for (const auto& t : extraTexts) ensureCodepointsBaked(t);
```

- [ ] **Step 4: Update `TextRenderer::init`'s convenience wrapper**

`init()` currently calls `bakeFonts()` with no args — this still compiles unchanged since `extraTexts` defaults to `{}` in the header declaration (the `.cpp` definition must NOT repeat the default argument — C++ only allows the default in one place, the header declaration already has it from Step 1... actually `bakeFonts`'s default `= {}` belongs on the declaration in the header, not on the out-of-line `.cpp` definition — verify the `.cpp`'s `bool TextRenderer::bakeFonts(std::vector<std::string> extraTexts) {` has NO `= {}` on it, only the header does). No code change needed in `init()` itself.

- [ ] **Step 5: Update `main.cpp`'s thread-spawn call site**

Find `std::thread fontThread(&TextRenderer::bakeFonts, &text);` in `src/app/main.cpp`. This still compiles as-is (binds the default-argument-free 0-arg call since `std::thread` requires all bound arguments explicit — check: `std::thread`'s constructor does NOT auto-apply default arguments for a member-function pointer target, since it deduces the exact parameter list from the pointer type, which is `bool(TextRenderer::*)(std::vector<std::string>)` — a `std::vector<std::string>` parameter, no default at the call site). This means Task 5/3's wiring must pass an explicit vector. Leave this line as-is for now (Task 3 changes it) — this task's own build check just needs it to compile with an explicit empty vector if it doesn't already:
```cpp
    std::thread fontThread(&TextRenderer::bakeFonts, &text, std::vector<std::string>{});
```
Apply this one-line change to `main.cpp` now so Task 2 builds standalone; Task 3 will replace the empty vector with the real showcase strings.

- [ ] **Step 6: Build and verify no regression**

```powershell
.\build.bat debug
```
Launch the app, confirm Text/Shapes/Widgets/Animation pages all still work exactly as before (this task adds no visible behavior yet — Task 3 wires it into a showcase).

- [ ] **Step 7: Commit**

```bash
git add src/text/text_renderer.h src/text/text_renderer.cpp src/app/main.cpp
git commit -m "Add font-fallback chain (ensureCodepointsBaked) for non-Latin codepoints"
```

---

### Task 3: Text page multi-script showcase (Chinese/Japanese/Korean lines)

**Files:**
- Modify: `src/app/page_text.h`
- Modify: `src/app/page_text.cpp`
- Modify: `src/app/main.cpp`

**Interfaces:**
- Consumes: `TextRenderer::bakeFonts(std::vector<std::string>)` (Task 2), `TextRenderer::drawText` (Task 1, now UTF-8-aware).
- Produces: `page_text.h` exposes 3 new `extern const char*` sample-string constants that `main.cpp` also reads (single source of truth, no string duplication between "what gets baked" and "what gets drawn").

- [ ] **Step 1: Add named sample-string constants to `page_text.h`**

In `src/app/page_text.h`, add above the `TextPage` class declaration:
```cpp
// Multi-script showcase strings. Declared here (not just inline in
// page_text.cpp) so main.cpp can pass the exact same strings to
// TextRenderer::bakeFonts()'s extraTexts, keeping "what gets baked" and
// "what gets drawn" as one source of truth.
extern const char* kChineseSample;
extern const char* kJapaneseSample;
extern const char* kKoreanSample;
```

- [ ] **Step 2: Define the constants and add the showcase lines in `page_text.cpp`**

In `src/app/page_text.cpp`, add near the top (after the includes):
```cpp
const char* kChineseSample = "你好，世界";
const char* kJapaneseSample = "こんにちは世界";
const char* kKoreanSample = "안녕하세요 세계";
```

Update the stale UTF-8 comment at the top of the file (from the earlier em-dash fix) — replace:
```cpp
// NOTE: em-dashes replaced with plain hyphens here -- TextRenderer::drawText
// is byte-oriented, not UTF-8-aware, so multi-byte UTF-8 characters like an
// em-dash render as garbled glyphs (same issue found and fixed in Task 6).
```
with:
```cpp
// NOTE: drawText is UTF-8-aware (see Task 1 of the multi-script plan); the
// hyphens below are a leftover style choice from before that fix, not a
// remaining workaround.
```

Add three more `Line` entries after the existing `{"Italic at 32pt", 32.0f, false, true}` entry:
```cpp
        {kChineseSample, 28.0f, false, false},
        {kJapaneseSample, 28.0f, false, false},
        {kKoreanSample, 28.0f, false, false},
```

- [ ] **Step 3: Wire the sample strings into `main.cpp`'s bake call**

In `src/app/main.cpp`, add `#include "page_text.h"` if not already present (it is, from Task 9 of the original plan), and change the Task 2 placeholder line:
```cpp
    std::thread fontThread(&TextRenderer::bakeFonts, &text, std::vector<std::string>{});
```
to:
```cpp
    std::thread fontThread(&TextRenderer::bakeFonts, &text,
        std::vector<std::string>{kChineseSample, kJapaneseSample, kKoreanSample});
```

- [ ] **Step 4: Build and visually verify**

```powershell
.\build.bat debug
```
Launch the app, navigate to the Text page. Confirm: the three new lines render real Chinese/Japanese/Korean glyphs (not empty boxes, not garbled bytes) below the existing Latin lines. Take a screenshot for verification evidence, per this project's established pattern. Confirm startup is still fast (no new multi-second stall — the fallback bake is a few dozen glyphs, comparable cost to the existing Bold/Italic bake, not a regression).

- [ ] **Step 5: Commit**

```bash
git add src/app/page_text.h src/app/page_text.cpp src/app/main.cpp
git commit -m "Add Chinese/Japanese/Korean showcase lines to the Text page"
```

---

### Task 4: Offline math atlas bake (spike + asset)

**Files:**
- Create: `fonts/math/font.msdf` (binary, generated by `atlas_gen`)
- Create: `fonts/math/atlas.rgba` (binary, generated by `atlas_gen`)

**Interfaces:**
- Consumes: `libs/firstparty/vulkan_font_engine/tools/atlas_gen` (host tool, built locally, not part of this app's CMake build), `fonts/lm/lmroman10-regular.otf`, `fonts/lm/lmroman10-bold.otf`, `fonts/lm/lmroman10-italic.otf` (already bundled), `fonts/lm-math/latinmodern-math.otf` (already bundled).
- Produces: two binary asset files that Task 5 loads via `MsdfFont::load(AssetReader&, metricsPath, atlasPath)`.

- [ ] **Step 1: Build the atlas_gen host tool**

```powershell
cd "C:\Users\incxiuefb\Documents\Files\clone\windows_UI_demo\.claude\worktrees\vulkan-bw-ui-demo\libs\firstparty\vulkan_font_engine\tools\atlas_gen"
pwsh build.ps1
```
Expected: `atlas_gen built: ...\tools\atlas_gen\build\atlas_gen.exe` printed, exe exists.

- [ ] **Step 2: Run it against this repo's four Latin Modern fonts**

```powershell
cd "C:\Users\incxiuefb\Documents\Files\clone\windows_UI_demo\.claude\worktrees\vulkan-bw-ui-demo"
New-Item -ItemType Directory -Force -Path fonts\math | Out-Null
.\libs\firstparty\vulkan_font_engine\tools\atlas_gen\build\atlas_gen.exe `
    fonts\lm\lmroman10-regular.otf fonts\lm\lmroman10-bold.otf fonts\lm\lmroman10-italic.otf `
    fonts\lm-math\latinmodern-math.otf fonts\math\font.msdf fonts\math\atlas.rgba
```
Expected: exits 0, `fonts\math\font.msdf` and `fonts\math\atlas.rgba` both exist and are nonzero-sized (tens of KB to a few MB is a reasonable range).

- [ ] **Step 3: Confirm the vendored API surface `mathbox`/`mathlayout` will need (de-risking check)**

```bash
grep -n "glyphPadEm\|mathConstants\|hasMath\|glyphByKey\|mathKey\|keyForStyle\|construction\|buildVStretch\|advanceKey\|layoutByKey" libs/firstparty/vulkan_font_engine/core/msdf.hh
```
Expected: all of these already present (confirmed during planning — `glyphPadEm()` at `msdf.hh:177`, `MathConstants` struct + accessors at `msdf.hh:31-70`, `hasMath()`/`mathConstants()`/`glyphByKey()`/`keyForStyle()`/`mathKey()`/`construction()`/`buildVStretch()`/`layoutByKey()` all present with matching signatures per the plan's research). If anything is unexpectedly missing, STOP and report — do not proceed to Task 5+ until resolved (add a minimal, clearly-commented accessor to the local submodule checkout if a small getter over an already-tracked field is missing; escalate if it's a larger gap).

- [ ] **Step 4: Sanity-load the baked atlas**

Write a throwaway smoke-test (do not commit it, just run it to verify the bake is loadable — delete the file afterward) at a scratch path, e.g. `C:\Users\incxiuefb\AppData\Local\Temp\claude\...\scratchpad\math_load_smoke.cpp`:
```cpp
#include "msdf.hh"
#include "asset_reader.hh"
#include <cstdio>
int main() {
    FileByteReader reader;
    MsdfFont font;
    bool ok = font.load(reader, "fonts/math/font.msdf", "fonts/math/atlas.rgba");
    printf("load: %s, hasMath: %s\n", ok ? "OK" : "FAIL", (ok && font.hasMath()) ? "true" : "false");
    return ok && font.hasMath() ? 0 : 1;
}
```
Compile it standalone against the vendored engine's `core` sources (mirror how `primitives_test`/`widgets_test` are built as standalone exes in this repo's `CMakeLists.txt` for the pattern) — or, simpler, temporarily add a throwaway `add_executable(math_load_smoke ...)` target to `CMakeLists.txt`, build+run it, confirm `load: OK, hasMath: true`, then revert the `CMakeLists.txt` change (this task doesn't add a permanent test target — Task 5 integrates the real loading path into the app itself, which IS the real verification going forward).

- [ ] **Step 5: Commit the baked assets**

```bash
git add fonts/math/font.msdf fonts/math/atlas.rgba
git commit -m "Bake offline MATH-table atlas (Latin Modern regular/bold/italic/math) via atlas_gen"
```

---

### Task 5: Math font loading + weight-slot-3 wiring

**Files:**
- Modify: `src/text/text_renderer.h`
- Modify: `src/text/text_renderer.cpp`

**Interfaces:**
- Consumes: `MsdfFont::load(AssetReader&, const char*, const char*)` returning `bool`, `MsdfTextRenderer::createResources(VkRenderPass, const MsdfFont&, int weightIdx)` (weightIdx 3), `MsdfTextRenderer::recordAtlasUpload(cmd, weightIdx)`, `MsdfTextRenderer::uploadGlyphQuads`/`draw` (all pre-existing, `MAX_FONT_WEIGHTS = 4` already supports a 4th slot).
- Produces: `MsdfFont& TextRenderer::mathFont()` (const accessor for `MathCanvas`, Task 6), `void TextRenderer::drawMathGlyph(uint32_t key, float x, float y, float sizePx, float gray)` (queues a math glyph quad, mirrors the existing Bold/Italic key-based emission pattern from Task 1), `bool TextRenderer::hasMathFont() const`.

- [ ] **Step 1: Add math font member and accessors to the header**

In `src/text/text_renderer.h`, add to the private section:
```cpp
    MsdfFont mathFont_;
    bool mathFontLoaded_ = false;
    std::vector<float> pendingMathVerts_;
```
Add to the public section:
```cpp
    // True once the offline-baked MATH atlas (fonts/math/font.msdf +
    // atlas.rgba) has been loaded and its GPU resources created (weight
    // slot 3). Loading happens inside finishGpuInit() (Step 2 below), so it
    // shares that method's existing fontsBaked()-gated call site in
    // main.cpp -- it doesn't strictly need to wait for the CJK/Bold/Italic
    // background bake (MsdfFont::load() just reads two pre-baked files and
    // uploads to GPU, no msdfgen rasterization), but piggybacking on the
    // existing gate avoids adding a third main.cpp readiness flag, and the
    // added wait is negligible (load() is fast) compared to the bake it's
    // riding along with.
    bool hasMathFont() const { return mathFontLoaded_; }
    const MsdfFont& mathFont() const { return mathFont_; }
    // Queues one math glyph (addressed by MsdfFont::mathKey()/glyphByKey()
    // key, NOT a codepoint) for the next draw() call. Only valid once
    // hasMathFont() is true.
    void drawMathGlyph(uint32_t key, float x, float y, float sizePx, float gray);
```

- [ ] **Step 2: Implement loading inside `finishGpuInit`**

In `src/text/text_renderer.cpp`, at the end of `TextRenderer::finishGpuInit` (after the existing three `createResources` calls, before `return true;`), add:
```cpp
    std::string exeDir = exeDirectory();
    std::string mathMetrics = exeDir + "\\fonts\\math\\font.msdf";
    std::string mathAtlas = exeDir + "\\fonts\\math\\atlas.rgba";
    if (mathFont_.load(assets_, mathMetrics.c_str(), mathAtlas.c_str())) {
        renderer_.createResources(renderPass, mathFont_, /*weightIdx=*/3);
        mathFontLoaded_ = true;
    }
```

- [ ] **Step 3: Record its atlas upload**

In `TextRenderer::recordAtlasUpload`, add after the existing three calls:
```cpp
    if (mathFontLoaded_) renderer_.recordAtlasUpload(cmd, 3);
```

- [ ] **Step 4: Implement `drawMathGlyph`**

Add to `text_renderer.cpp`, near `drawText`:
```cpp
void TextRenderer::drawMathGlyph(uint32_t key, float x, float y, float sizePx, float gray) {
    if (!mathFontLoaded_) return;
    GlyphQuad q;
    mathFont_.layoutByKey(key, x, y, sizePx, q);
    if (!q.draw) return;
    auto& out = pendingMathVerts_;
    auto vert = [&](float vx, float vy, float u, float v) {
        out.push_back(vx); out.push_back(vy); out.push_back(u); out.push_back(v);
        out.push_back(gray); out.push_back(gray); out.push_back(gray); out.push_back(1.0f);
    };
    vert(q.x0, q.y0, q.u0, q.v0); vert(q.x1, q.y0, q.u1, q.v0); vert(q.x1, q.y1, q.u1, q.v1);
    vert(q.x0, q.y0, q.u0, q.v0); vert(q.x1, q.y1, q.u1, q.v1); vert(q.x0, q.y1, q.u0, q.v1);
}
```
Note: `layoutByKey`'s `penX` parameter is reused here as an absolute `x` position (not pen-advance-based) — math glyph placement is computed entirely by the ported `mathbox` layout algorithm (Task 8), which calls this once per already-positioned glyph, not as a left-to-right text run. This matches how `mathbox.cc`'s own `c.mathGlyph()` calls are used (absolute positions, not pen advance).

- [ ] **Step 5: Flush `pendingMathVerts_` in `draw()`**

In `TextRenderer::draw`, add after the existing Italic block:
```cpp
    if (!pendingMathVerts_.empty()) {
        uint32_t count = (uint32_t)(pendingMathVerts_.size() / MsdfFont::FLOATS_PER_VERT);
        renderer_.uploadGlyphQuads(pendingMathVerts_.data(), count, 3);
        renderer_.draw(cmd, renderer_.vertOffset(3), count, 0, 0, 0, 0, extent.width, extent.height, 3);
    }
```
and add `pendingMathVerts_.clear();` alongside the other two `.clear()` calls at the end of the function.

- [ ] **Step 6: Build and verify no regression**

```powershell
.\build.bat debug
```
Launch the app, confirm all four existing pages still work — this task adds no visible content yet (nothing calls `drawMathGlyph` until Task 10), it only wires the loading/plumbing. Confirm no crash/`fatal()` message box on startup (a missing/corrupt `fonts/math/*` would silently skip math font loading per Step 2's `if` check, not fail the whole app — that's intentional: math is an additive feature, its absence shouldn't break Regular/Bold/Italic text).

- [ ] **Step 7: Commit**

```bash
git add src/text/text_renderer.h src/text/text_renderer.cpp
git commit -m "Load offline MATH atlas into weight slot 3 (drawMathGlyph plumbing)"
```

---

### Task 6: `MathCanvas` adapter

**Files:**
- Create: `src/math/math_canvas.h`
- Create: `src/math/math_canvas.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `TextRenderer::mathFont()`, `TextRenderer::drawMathGlyph`, `TextRenderer::drawText`/`textWidth` (Roman path, for any plain-upright text the ported layout code needs), `PrimitiveBatch::pushRect`.
- Produces: `class MathCanvas` — the exact adapter surface `mathbox`/`mathlayout` (Tasks 8-9) call, with `Color` replaced by `float gray` per this plan's Global Constraints.

- [ ] **Step 1: Write `src/math/math_canvas.h`**

```cpp
#pragma once
#include "../text/text_renderer.h"
#include "../gfx/primitives.h"
#include <string_view>

// Adapter implementing the small surface the ported mathbox/mathlayout code
// (originally written against a Canvas class from a different, unavailable
// engine — see mathbox.h's header comment) needs, routed to this app's
// existing PrimitiveBatch/TextRenderer. Grayscale-only: every method takes
// a single `gray` float, not a Color (this app has no chroma anywhere).
class MathCanvas {
 public:
    MathCanvas(PrimitiveBatch& batch, TextRenderer& text) : batch_(batch), text_(text) {}

    const MsdfFont* msdfFont() const { return text_.hasMathFont() ? &text_.mathFont() : nullptr; }
    void text(std::string_view s, float x, float y, float size, float gray) {
        text_.drawText(s, x, y, size, gray);
    }
    float textWidth(std::string_view s, float size) const { return text_.textWidth(s, size); }
    void rect(float x, float y, float w, float h, float gray) { batch_.pushRect(x, y, w, h, gray); }
    void mathGlyph(uint32_t key, float x, float y, float size, float gray) {
        text_.drawMathGlyph(key, x, y, size, gray);
    }

 private:
    PrimitiveBatch& batch_;
    TextRenderer& text_;
};
```

- [ ] **Step 2: Write `src/math/math_canvas.cpp`**

```cpp
#include "math_canvas.h"
// MathCanvas is fully header-defined (all methods are trivial one-line
// routes to existing primitives); this file exists so the CMake source
// list has a real translation unit, matching this repo's established
// pattern (see src/app/nav.cpp).
```

- [ ] **Step 3: Add to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE src/math/math_canvas.cpp)
```

- [ ] **Step 4: Build**

```powershell
.\build.bat debug
```
Expected: clean build, no new warnings. (`MathCanvas` isn't used by anything yet — this task just confirms it compiles standalone against the real headers.)

- [ ] **Step 5: Commit**

```bash
git add src/math/math_canvas.h src/math/math_canvas.cpp CMakeLists.txt
git commit -m "Add MathCanvas adapter (grayscale, routes to PrimitiveBatch/TextRenderer)"
```

---

### Task 7: Port `core/math` (AST + lexer + parser) with unit tests

**Files:**
- Create: `src/math/ast.h`
- Create: `src/math/lexer.h`
- Create: `src/math/lexer.cpp`
- Create: `src/math/parser.h`
- Create: `src/math/parser.cpp`
- Test: `src/math/parser_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from this app (confirmed zero platform/rendering coupling in the source).
- Produces: `mathx::Node` (tagged struct, `Kind` enum `Num/Const/Var/Neg/Add/Sub/Mul/Div/Pow/Call`), `mathx::NodePtr = std::unique_ptr<Node>`, `bool mathx::tokenize(const std::string&, std::vector<Token>&, bool implicitMul = false)`, `mathx::ParseResult mathx::parse(const std::vector<Token>&)` returning `{NodePtr root; bool ok;}`. Task 9 (`buildAst`) consumes `mathx::Node`/`NodePtr` directly.

- [ ] **Step 1: Copy and adapt `ast.hh` → `src/math/ast.h`**

Read `C:\Users\incxiuefb\Documents\Files\clone\calculator\core\math\ast.hh` in full (33 lines) and copy it verbatim to `src/math/ast.h`, with only these changes:
- Add a license/provenance comment at the top: `// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\core\math\ast.hh (same author, AGPLv3).`
- Change the `#pragma once` header guard style if it differs from this repo's convention (this repo uses `#pragma once` everywhere — keep it if the source already uses it, no change needed).
- No other changes — this file has zero platform coupling per the plan's research.

- [ ] **Step 2: Copy and adapt `lexer.hh`/`lexer.cc` → `src/math/lexer.h`/`lexer.cpp`**

Read `C:\Users\incxiuefb\Documents\Files\clone\calculator\core\math\lexer.hh` and `lexer.cc` in full (31 + 96 lines) and copy verbatim to `src/math/lexer.h`/`src/math/lexer.cpp`, adding the same provenance comment. Update the `#include "ast.hh"`-style include (if `lexer.hh` includes anything from the same directory) to match this repo's relative-include convention (`#include "ast.h"`, matching the renamed `.h` extension from Step 1) and the include in `lexer.cpp` for its own header to `#include "lexer.h"`.

- [ ] **Step 3: Copy and adapt `parser.hh`/`parser.cc` → `src/math/parser.h`/`parser.cpp`**

Read `C:\Users\incxiuefb\Documents\Files\clone\calculator\core\math\parser.hh` and `parser.cc` in full (19 + 127 lines) and copy verbatim to `src/math/parser.h`/`src/math/parser.cpp`, same provenance comment, same include-extension fixups (`ast.hh`→`ast.h`, `lexer.hh`→`lexer.h`, `parser.hh`→`parser.h`).

- [ ] **Step 4: Write the failing test**

```cpp
// src/math/parser_test.cpp
#include "parser.h"
#include "lexer.h"
#include <cassert>
#include <cstdio>

static mathx::ParseResult parseStr(const std::string& s) {
    std::vector<mathx::Token> toks;
    bool lexOk = mathx::tokenize(s, toks);
    assert(lexOk);
    return mathx::parse(toks);
}

int main() {
    // Basic arithmetic with correct precedence
    auto r1 = parseStr("1+2*3");
    assert(r1.ok && r1.root->kind == mathx::Node::Add);
    assert(r1.root->kids[1]->kind == mathx::Node::Mul); // "2*3" binds tighter than "1+"

    // Right-associative power
    auto r2 = parseStr("2^3^2");
    assert(r2.ok && r2.root->kind == mathx::Node::Pow);
    assert(r2.root->kids[1]->kind == mathx::Node::Pow); // 2^(3^2), not (2^3)^2

    // Function call
    auto r3 = parseStr("sqrt(4)");
    assert(r3.ok && r3.root->kind == mathx::Node::Call);
    assert(r3.root->name == "sqrt");
    assert(r3.root->kids[0]->kind == mathx::Node::Num);
    assert(r3.root->kids[0]->num == 4.0);

    // Constant recognition
    auto r4 = parseStr("pi");
    assert(r4.ok && r4.root->kind == mathx::Node::Const);
    assert(r4.root->name == "pi");

    // Unary minus
    auto r5 = parseStr("-5");
    assert(r5.ok && r5.root->kind == mathx::Node::Neg);

    // Parenthesized division
    auto r6 = parseStr("1/(2+3)");
    assert(r6.ok && r6.root->kind == mathx::Node::Div);
    assert(r6.root->kids[1]->kind == mathx::Node::Add);

    // Syntax error: leftover token
    auto r7 = parseStr("1 2");
    assert(!r7.ok);

    printf("parser_test: OK\n");
    return 0;
}
```

- [ ] **Step 5: Add test target and run to verify it fails first**

```cmake
add_executable(parser_test src/math/lexer.cpp src/math/parser.cpp src/math/parser_test.cpp)
target_include_directories(parser_test PRIVATE src/math)
```

```powershell
cmake -G Ninja -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug --target parser_test
```
Expected: FAILS before Steps 1-3 are done (files don't exist); after Steps 1-3 are done, this should already PASS on the first build since the ported code is a verbatim, previously-working port — if any assertion fails, the port introduced a bug, debug against the original `calculator` source rather than adjusting the test.

- [ ] **Step 6: Run and confirm it passes**

```powershell
cmake --build build_debug --target parser_test
.\build_debug\parser_test.exe
```
Expected: `parser_test: OK`.

- [ ] **Step 7: Add sources to the main app target**

```cmake
target_sources(windows_ui_demo PRIVATE src/math/lexer.cpp src/math/parser.cpp)
```

- [ ] **Step 8: Build the main app to confirm no regressions**

```powershell
.\build.bat debug
```
Expected: clean build (this task adds no app-visible behavior yet — Task 10 wires `parse()` into the Math page).

- [ ] **Step 9: Commit**

```bash
git add src/math/ast.h src/math/lexer.h src/math/lexer.cpp src/math/parser.h src/math/parser.cpp src/math/parser_test.cpp CMakeLists.txt
git commit -m "Port math expression lexer/parser/AST from calculator repo, with unit tests"
```

---

### Task 8: Port `mathbox` (TeX box-layout engine)

**Files:**
- Create: `src/math/mathbox.h`
- Create: `src/math/mathbox.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `MathCanvas` (Task 6), `MsdfFont`/`MathConstants`/`MsdfGlyph`/`MathConstruction`/`VStretch` (vendored engine, `libs/firstparty/vulkan_font_engine/core/msdf.hh` — all confirmed present with matching signatures during Task 4's de-risking check).
- Produces: `mb::Node` (tagged struct, `Kind` enum `HBox/Text/Glyph/Frac/Radical/Script/Delim/Placeholder/Caret`), `mb::NodePtr`, `mb::MathStyle` enum (`Display/Text/Script/ScriptScript`), `mb::Box` struct, `Box mb::layout(MathCanvas&, Node&, float displaySize, MathStyle st = MathStyle::Display)`, `void mb::draw(MathCanvas&, const Node&, float x, float yAxis, float gray)`. Task 9 (`buildAst`) constructs `mb::Node` trees; Task 10 (Math page) calls `layout`/`draw`.

- [ ] **Step 1: Read the source files in full**

Read `C:\Users\incxiuefb\Documents\Files\clone\calculator\app\src\main\cpp\mathbox.hh` (123 lines) and `mathbox.cc` (697 lines) completely before starting the port — this is the largest single file in this plan; understand the full structure (the `Node` struct, the 9 `Kind` values, `MathStyle`/`styleScale`, the `kGlue[8][8]` spacing table, `layout()`, `draw()`, and the shared helpers `axisFrac`/`atomAbove`/`atomBelow`/`axisText`/`mathItalicCp`/`radicalMeasure`/`radicalDraw`/`dashedRect`/`openDelim`/`closeDelim`/`delimColor`) before writing anything.

- [ ] **Step 2: Copy `mathbox.hh` → `src/math/mathbox.h`, adapting the surface**

Copy the file verbatim, then apply these exact changes:
- Add the provenance comment (same pattern as Task 7).
- Change `#include "canvas.hh"` to `#include "math_canvas.h"`.
- Change every `Canvas&`/`Canvas &` parameter type to `MathCanvas&`.
- Remove the `Color` type entirely: change every `Color`-typed field/parameter to `float` (representing gray). Specifically: `Node::color` (a `Color`) becomes `float gray = 0.9f;`, `Node::hasColor` stays (still meaningful — "does this leaf override the inherited gray"), and any `draw()`/helper signature taking `const Style& s` (if `Style` is a small struct bundling a base `Color` plus other draw-time settings — read the actual struct definition and reduce it to just what's needed for a grayscale demo, most likely just a `float gray` float, possibly dropped as a parameter entirely if `Style` carried nothing else useful) — inspect the real `Style` struct in the source before deciding; if it's Color-only, replace `const Style&` parameters with a plain `float gray` parameter throughout.
- Drop `Node::negative` and the "cycled paren color by nesting level" behavior tied to `Node::level` if `level`'s ONLY use is color-cycling (verify by reading `mathbox.cc`'s uses of `level` — if it's ALSO used for something structural like `Delim` glyph selection depth unrelated to color, keep `level` but drop only the color-selection logic that reads it).
- Keep everything else (the 9 `Kind` values, `MathStyle`, `Box`, `layout`/`draw` signatures with `Canvas`→`MathCanvas` substituted, all the helper function declarations) unchanged.

- [ ] **Step 3: Copy `mathbox.cc` → `src/math/mathbox.cpp`, adapting the implementation**

Copy the file verbatim, then apply the corresponding implementation-side changes:
- `#include "mathbox.hh"` → `#include "mathbox.h"`.
- Every `c.msdfFont()`, `c.text(...)`, `c.textWidth(...)`, `c.rect(...)`, `c.mathGlyph(...)` call site stays as-is (these exact method names/signatures are what `MathCanvas` implements — verify each call site's argument types match `MathCanvas`'s methods exactly; the only mismatch expected is `Color`-typed color arguments, which become `float gray` arguments — thread through whatever `float`/`Node::gray` value the surrounding code already computed in place of the old `Color`).
- Anywhere a `Color` was constructed/manipulated (e.g. a "dim ghost" alpha-blend, a "cycled paren color" lookup table) — replace with the simplest reasonable grayscale equivalent: a dimmer `gray` value (e.g. multiply by ~0.5 for a "ghost"/ inactive element) rather than dropping the visual distinction entirely, unless the distinction genuinely has no meaning without color (e.g. a 6-color paren-cycling palette collapses to "just draw all parens at the same gray" — that's fine, it's cosmetic).
- `setClip` — per the plan's research, this is called only by the calculator's own `main.cc` caller, not by `mathbox.cc`/`mathlayout.cc` themselves; if that holds after reading the actual file, no adapter method is needed for it and no change is required here.

- [ ] **Step 4: Add to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE src/math/mathbox.cpp)
```

- [ ] **Step 5: Build**

```powershell
.\build.bat debug
```
Fix any compile errors by re-checking the exact `MathCanvas` method signatures against what `mathbox.cpp` calls — this is expected to take a few iterations given the file's size; work through errors top-to-bottom rather than guessing, and re-read the relevant section of the original `mathbox.cc` whenever a call site's intent is unclear.

- [ ] **Step 6: Commit**

```bash
git add src/math/mathbox.h src/math/mathbox.cpp CMakeLists.txt
git commit -m "Port mathbox TeX box-layout engine (grayscale-adapted) from calculator repo"
```

(No unit test for this task — it's GPU-metrics-adjacent, dependent on a real `MsdfFont`/`MathConstants` instance; verified visually once Tasks 9-10 wire it into the Math page, matching this repo's established convention for GPU-adjacent code.)

---

### Task 9: Port the AST-half of `mathlayout` (`buildAst`/`fitAst`/`drawAstAt`)

**Files:**
- Create: `src/math/math_layout.h`
- Create: `src/math/math_layout.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `mathx::Node`/`NodePtr` (Task 7), `mb::Node`/`NodePtr`/`layout`/`draw` (Task 8), `MathCanvas` (Task 6).
- Produces: `struct AstFit { float width, above, below, size; }`, `AstFit fitAst(MathCanvas&, const mathx::Node& root, float maxSize, float maxWidth)`, `void drawAstAt(MathCanvas&, const mathx::Node& root, float rightX, float yAxis, float size, float gray = 0.9f)`. Task 10 (Math page) calls both.

- [ ] **Step 1: Read the source files in full**

Read `C:\Users\incxiuefb\Documents\Files\clone\calculator\app\src\main\cpp\mathlayout.hh` (85 lines) and `mathlayout.cc` (747 lines) completely. Identify and mentally separate the two builders: `buildEdRow`/`drawEditor` (editor-tree-based, live-input-specific — **do not port this half**) versus `buildAst`/`fitAst`/`drawAstAt` and their shared helpers (`wrapAst`, `prec`, `delimDepth`, `wrapDepth`, `isHalf`) — **port only this half**. Also note but do not port `buildEqLabelIR`/`drawKeyIcon` (UI-chrome-specific, not needed for a static showcase page).

- [ ] **Step 2: Copy and trim `mathlayout.hh` → `src/math/math_layout.h`**

Copy only the declarations relevant to the AST-half: the `AstFit` struct, `fitAst`, `drawAstAt` (and `buildAst` if it's declared in the header rather than being file-local to the `.cc` — check). Omit every editor-tree-related declaration (`buildEdRow`, `drawEditor`, and anything referencing `calcedit::` types). Add the provenance comment. Change `#include "canvas.hh"` → `#include "math_canvas.h"` (and `Canvas&` → `MathCanvas&` throughout), change `#include "mathbox.hh"` → `#include "mathbox.h"`, change `#include "ast.hh"`-style math-AST include → `#include "ast.h"`.

- [ ] **Step 3: Copy and trim `mathlayout.cc` → `src/math/math_layout.cpp`**

Copy only the AST-half functions and their private helpers (`buildAst`, `wrapAst`, `prec`, `delimDepth`, `wrapDepth`, `isHalf`, `fitAst`, `drawAstAt`) — omit `buildEdRow`, `drawEditor`, `buildEqLabelIR`, `drawKeyIcon`, and any helper used only by those. Apply the same `Canvas`→`MathCanvas`, `Color`→`float gray` adaptations as Task 8. Fix includes to match (`mathbox.h`, `math_layout.h`, `ast.h`, `math_canvas.h`).

- [ ] **Step 4: Add to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE src/math/math_layout.cpp)
```

- [ ] **Step 5: Build**

```powershell
.\build.bat debug
```
Fix compile errors the same way as Task 8 — this file is smaller (~90 lines of the ~747 total, since the editor-half is excluded), so this should be faster to stabilize.

- [ ] **Step 6: Commit**

```bash
git add src/math/math_layout.h src/math/math_layout.cpp CMakeLists.txt
git commit -m "Port AST-to-mathbox builder (buildAst/fitAst/drawAstAt) from calculator repo"
```

---

### Task 10: Math showcase page + nav wiring

**Files:**
- Create: `src/app/page_math.h`
- Create: `src/app/page_math.cpp`
- Modify: `src/app/nav.h`
- Modify: `src/app/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `mathx::tokenize`/`mathx::parse` (Task 7), `mathlayout::fitAst`/`drawAstAt` (Task 9), `MathCanvas` (Task 6).
- Produces: `class MathPage { public: void draw(MathCanvas& canvas, Rect area, float uiScaleFactor); };`, `Page::Math` enum value.

- [ ] **Step 1: Write `src/app/page_math.h`**

```cpp
#pragma once
#include "../math/math_canvas.h"
#include "../ui/layout.h"

class MathPage {
 public:
    void draw(MathCanvas& canvas, Rect area, float uiScaleFactor);
};
```

- [ ] **Step 2: Write `src/app/page_math.cpp`**

```cpp
#include "page_math.h"
#include "../math/parser.h"
#include "../math/lexer.h"
#include "../math/math_layout.h"

void MathPage::draw(MathCanvas& canvas, Rect area, float uiScaleFactor) {
    struct Example { const char* expr; };
    Example examples[] = {
        {"1/(2+sqrt(3))"},
        {"x^2+2*x+1"},
        {"sqrt(2)/2"},
        {"(1+2)*(3-4)/5"},
    };
    float y = area.y + 60.0f * uiScaleFactor;
    float size = 40.0f * uiScaleFactor;
    for (auto& ex : examples) {
        std::vector<mathx::Token> toks;
        if (!mathx::tokenize(ex.expr, toks)) continue;
        mathx::ParseResult pr = mathx::parse(toks);
        if (!pr.ok || !pr.root) continue;
        mathlayout::AstFit fit = mathlayout::fitAst(canvas, *pr.root, size, area.w - 80.0f * uiScaleFactor);
        mathlayout::drawAstAt(canvas, *pr.root, area.x + 40.0f * uiScaleFactor + fit.width,
                              y + fit.above, fit.size, 0.9f);
        y += fit.above + fit.below + 40.0f * uiScaleFactor;
    }
}
```
(Exact field names on `AstFit`/exact parameter order on `drawAstAt` must match whatever Task 9 actually produced when porting `mathlayout.hh` — read `src/math/math_layout.h` before writing this file and adjust the call sites to match precisely; the shapes above reflect the plan's Task 9 interface declaration, but Task 9's implementer may have found the real source's exact field names differ slightly during the port, in which case this task must match the real, already-committed `math_layout.h`, not this plan's prose.)

- [ ] **Step 3: Add `Page::Math` to `nav.h`'s enum**

In `src/app/nav.h`, change:
```cpp
enum class Page { Text, Shapes, Widgets, Animation };
```
to:
```cpp
enum class Page { Text, Shapes, Widgets, Animation, Math };
```

- [ ] **Step 4: Extend `TopNav` to 5 tabs**

In `src/app/nav.h`, change `std::array<Button, 4> tabs_;` to `std::array<Button, 5> tabs_;`, and update the constructor's member-initializer list to add a 5th `Button`:
```cpp
    TopNav()
        : tabs_{Button{20.0f, 20.0f, 180.0f, 44.0f},
                Button{210.0f, 20.0f, 180.0f, 44.0f},
                Button{400.0f, 20.0f, 180.0f, 44.0f},
                Button{590.0f, 20.0f, 180.0f, 44.0f},
                Button{780.0f, 20.0f, 180.0f, 44.0f}} {}
```
Change every `for (int i = 0; i < 4; i++)` in `updateLayout`, `update`, and `draw` to `for (int i = 0; i < 5; i++)`. In `draw`, change:
```cpp
        const char* labels[4] = {"Text", "Shapes & Curves", "Widgets", "Animation"};
```
to:
```cpp
        const char* labels[5] = {"Text", "Shapes & Curves", "Widgets", "Animation", "Math"};
```

- [ ] **Step 5: Wire `MathPage` into `main.cpp`**

Add `#include "page_math.h"`. Declare `MathPage mathPage;` alongside the other page objects. In the `switch (currentPage)` block, add:
```cpp
                case Page::Math: {
                    MathCanvas canvas(batch, text);
                    mathPage.draw(canvas, nav.contentArea(), uiScaleFactor);
                    break;
                }
```
(construct `MathCanvas` locally per-frame — it's a lightweight reference-holding wrapper, no owned state, consistent with how `MathCanvas`'s constructor was designed in Task 6).

- [ ] **Step 6: Add sources to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE src/app/page_math.cpp)
```

- [ ] **Step 7: Build and manually verify all five pages**

```powershell
.\build.bat debug
```
Launch the app. Confirm: 5 nav tabs render, evenly spaced, none overlapping or overflowing the window at default size. Click through all five tabs — Text/Shapes/Widgets/Animation still work exactly as before (no regression), and the new Math tab shows four typeset equations: a fraction with a visible horizontal bar, an exponent (raised, smaller), a radical (√ symbol with a vinculum line over the radicand), and a nested parenthesized expression — all in grayscale, with plausible TeX-like spacing (not overlapping, not touching the bar/radical incorrectly). Resize the window on the Math page; confirm equations rescale/reflow without crashing or producing garbage. Take screenshots as verification evidence, per this project's established pattern.

- [ ] **Step 8: Commit**

```bash
git add src/app/page_math.h src/app/page_math.cpp src/app/nav.h src/app/main.cpp CMakeLists.txt
git commit -m "Add Math showcase page (5th nav tab) with fraction/exponent/radical examples"
```
