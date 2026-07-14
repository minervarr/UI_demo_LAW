# Multi-Script Text + Math Typesetting Design

## Context

`windows_ui_demo` currently renders only ASCII Latin text (`TextRenderer::drawText` iterates raw bytes, not UTF-8 codepoints) across three real styles (Regular/Bold/Italic, all baked from the Latin Modern family at runtime). The user added ~130 additional font files spanning CJK (Fandol, Haranoaji, Unfonts-core), a math-table font (Latin Modern Math), and more Latin Modern weights. This design covers two features to make use of them:

- **Multi-script text rendering**: proper UTF-8 decoding plus a font-fallback chain so codepoints the primary Latin font doesn't cover (CJK, etc.) render via the appropriate bundled font instead of dropping/garbling.
- **Math typesetting**: a new showcase page rendering real mathematical notation (fractions, exponents, radicals) via a ported TeX-style box-layout engine, reusing the vendored `vulkan_font_engine`'s (currently unused) OpenType MATH table support.

Both extend the existing Text-rendering subsystem but are architecturally independent and are specified/planned/reviewed as separate task groups.

## Feature A: Multi-Script Text Rendering

### Goal

`TextRenderer::drawText` correctly decodes and renders arbitrary UTF-8 text, falling back through a small ordered list of bundled fonts for codepoints the primary Latin Modern face doesn't cover — without introducing runtime stalls or re-litigating the startup-hang fix already in place.

### Design

**UTF-8 decoding.** Replace `drawText`'s `for (char c : text)` byte loop with the vendored engine's existing `utf8::nextCodepoint(std::string_view, size_t&)` (`libs/firstparty/vulkan_font_engine/core/utf8.hh`) — already handles malformed/truncated sequences gracefully (returns U+FFFD, always advances), already used elsewhere in the same submodule (`font.cc`, `msdf.cc`'s `textWidth`). This is a drop-in replacement for the character-iteration loop in both the Roman fast path and the Bold/Italic key-resolution path.

**Font-fallback chain.** New method `TextRenderer::ensureCodepointsBaked(std::string_view text)`:
1. Iterate `text`'s codepoints via `utf8::nextCodepoint`.
2. For each codepoint, check `font_.hasCodepoint(cp)`. If already covered (by the primary Latin Modern Roman/Bold/Italic bake, or a prior fallback bake), skip.
3. Otherwise, try a fixed, ordered fallback list — `fonts/fandol/FandolHei-Regular.otf`, `fonts/haranoaji/HaranoAjiGothic-Regular.otf`, `fonts/unfonts-core/UnDotum.ttf` — calling `font_.bakeCodepoints(assets_, path, {cp})` on each until one returns nonzero (glyph found) or the list is exhausted. `bakeCodepoints` already skips codepoints another call already covered, so calling it per-missing-codepoint across multiple fallback fonts costs nothing extra for hits from an earlier font in the list.

This reuses `bakeCodepoints`'s existing "append fallback glyphs to the shared default-face table" design — no new `FontStyle` slot, no new atlas texture, no change to weight-slot plumbing. Fallback glyphs render through the same Roman `emitGlyph` path already used for Regular text (they're default-face glyphs, not style-specific).

**Timing.** `TextRenderer::bakeFonts()` (the existing background-thread phase from the startup-hang fix) calls `ensureCodepointsBaked` on every string the showcase pages will draw — a small fixed list of demo strings passed in or hardcoded at the call site — so all fallback glyphs are ready before the first frame needs them. `ensureCodepointsBaked` itself stays public and safe to call from any thread that isn't concurrently touching `font_`/`assets_` from elsewhere (same threading contract as `bakeFonts()`), so it remains available for future dynamic-text use, accepting a bake stall for genuinely new, previously-unseen codepoints called outside the startup phase — that's an inherent, documented tradeoff of lazy baking, not a bug.

**Showcase.** Extend the Text page with three new lines: a short Chinese phrase (Fandol), a short Japanese phrase (Haranoaji), a short Korean phrase (Unfonts-core) — each only pulling in the handful of codepoints it actually displays, not full-charset coverage.

### Testing

No unit test for the rendering itself (consistent with this app's established convention — GPU/font work verified by running the app). `ensureCodepointsBaked`'s fallback-selection LOGIC (which font wins for a given codepoint, skip-already-covered behavior) is pure and could be unit-tested if it's factored to not require live `MsdfFont`/`FileByteReader` state — assess feasibility during implementation; if it requires real font I/O to test meaningfully, skip a unit test and rely on visual verification, matching this app's existing GPU-adjacent-code convention.

## Feature B: Math Typesetting

### Goal

A new "Math" page in the top nav, showing several hardcoded example equations (fractions, exponents, radicals, nested/parenthesized expressions) typeset via a real TeX-style box-layout algorithm reading OpenType MATH table metrics — not a hand-positioned approximation.

### Design

**Math atlas (offline, one-time bake).** The vendored engine's OpenType MATH table parsing only exists on its offline `atlas_gen` host-tool path (`libs/firstparty/vulkan_font_engine/tools/atlas_gen`) — the runtime `generate()`/`addStyle()` path used for Regular/Bold/Italic does not parse MATH data at all. Build `atlas_gen` (`pwsh tools/atlas_gen/build.ps1`), run it once against the four Latin Modern OTFs already in `fonts/` (regular/bold/italic/math), and commit the resulting `font.msdf` (v2) + `atlas.rgba` pair as bundled assets (same pattern as the vendored `.otf` files — binary assets checked into the repo, not generated at app build/run time).

**Math font loading.** A second, independent `MsdfFont` instance (call it `mathFont_`, owned by a new small component — see below) loads the pre-baked pair via `MsdfFont::load()` at startup. This is a fast disk-read-plus-GPU-upload, not rasterization, so it runs on the main thread during the existing GPU-init phase — no new background-threading concerns. It is kept separate from the Regular/Bold/Italic `MsdfFont` (`TextRenderer`'s `font_`) because the offline `load()` path and the runtime `generate()`/`addStyle()` path populate mutually-exclusive internal state and aren't designed to share one instance.

**Rendering plumbing.** The math font gets its own weight slot (slot 3, since `MAX_FONT_WEIGHTS = 4` and Regular/Bold/Italic already occupy 0/1/2) on the existing shared `MsdfTextRenderer`, reusing the established `createResources`/`recordAtlasUpload`/`uploadGlyphQuads`/`draw` pattern rather than standing up a second full text-rendering pipeline.

**Adapter (`MathCanvas`).** A small new shim class implementing the handful of methods the ported layout code needs, calling into our existing primitives:
- `msdfFont()` → `mathFont_`
- `text(s, x, y, size, color)` / `textWidth(s, size)` → existing `TextRenderer` Roman-path calls (for any plain-upright text mathbox needs, e.g. multi-digit numerals)
- `rect(x, y, w, h, color)` → `PrimitiveBatch::pushRect` (fraction bars, radical vinculum, etc. — all filled rectangles per the ported engine's own drawing convention)
- `mathGlyph(key, x, y, size, color)` → a new key-based glyph-quad emission path against `mathFont_`/weight-slot 3, mirroring the Bold/Italic key-resolution path already built for `drawText`

**Ported logic** (from `C:\Users\incxiuefb\Documents\Files\clone\calculator`, same author, AGPLv3 — noting the license for the record since it differs from whatever license this repo uses, if any is ever declared):
- `core/math/{ast.hh, lexer.hh/.cc, parser.hh/.cc}` — ported verbatim into a new `src/math/` directory. Confirmed zero platform/rendering coupling; pure C++17 recursive-descent parser (`+ - * / ^`, unary, parens, `sin/cos/tan/ln/log/sqrt`, `pi`/`e` constants).
- `mathbox.hh/.cc` (box-layout IR and the TeX spacing/style-scaling algorithm) and the AST-half of `mathlayout.hh/.cc` (`buildAst`, `fitAst`, `drawAstAt`, and their private helpers) — ported with the `Canvas`/`MsdfFont` call sites adapted to `MathCanvas` and our vendored engine's actual API. The editor/live-input half of `mathlayout` (`buildEdRow`, `drawEditor`) is explicitly **not** ported — out of scope, since this is a static showcase, not an interactive input widget.

**Math page.** `page_math.h/.cpp`, following the existing page pattern (`updateLayout`/`draw`), rendering several example equations via `parse()` → `buildAst()` → `mathbox::layout()` → `mathbox::draw()`. Content is whatever the ported grammar actually supports well: a fraction, an exponent, a radical, a nested/parenthesized expression. (Not claiming subscript support — the calculator's own `buildAst` doesn't emit subscripts from its grammar, so this design doesn't either.)

### De-risking

The calculator's `mathbox.cc`/`mathlayout.cc` call several `MsdfFont`/`MathConstants` methods (`glyphPadEm()`, the full `MathConstants` accessor set, `buildVStretch`, `construction`, `advanceKey`, a `Color` type) that were inventoried from call sites, not from reading the calculator's own vendored engine copy directly (that submodule isn't checked out in the calculator clone). Before committing to the full port, an early implementation task builds `atlas_gen`, bakes the math atlas, and greps **our** submodule's actual headers to confirm every needed API exists with matching signatures. Where something is missing (e.g. `glyphPadEm()`, plausibly just `distanceRange_ / sizePxEm_` over fields the engine already tracks internally), add a minimal, clearly-documented public accessor to our local submodule checkout rather than blocking — a small, additive, non-invasive change (a one-line getter), not a fork of the engine's logic. This keeps the vendored engine's behavior unchanged for existing callers.

### Testing

`core/math`'s ported lexer/parser is pure logic with no platform dependency — gets real unit tests (tokenization, precedence, function/constant recognition), consistent with this app's convention for pure-logic code. `mathbox`/`mathlayout`'s box-layout math is GPU-adjacent (reads font metrics, produces screen-space boxes) — verified by running the app and visually confirming the four example equations render with plausible TeX-like spacing/positioning (fraction bar centered on the math axis, radical vinculum covering the radicand, exponent raised and shrunk), matching this app's established "no automated test suite for rendering, verified by running the app" convention.

## Out of scope

- Interactive equation input (typing an expression) — static showcase only, per user decision.
- Subscripts, summation/integral notation, matrices — not supported by the ported grammar; not claimed.
- Full-charset CJK coverage — only the specific codepoints the showcase strings display are baked.
- Simultaneous bold+italic (pre-existing limitation from the earlier bold/italic fix, unrelated to this design).
