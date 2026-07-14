# Extract `src/math/` into a standalone, reusable `mathcore` library

## Context

`windows_ui_demo`'s Math page currently uses `src/math/` (ast/lexer/parser + mathbox TeX-layout + math_layout), ported from the `calculator` Android app, but only the minimal "type a static string once, typeset it" slice was brought over. The user now wants this to become a genuinely complete, fast, reusable math library — one that could later power a Desmos/GeoGebra-style app or a full PC port of the source calculator app, not just this one demo page. That means: a real separable library (not source files baked into one exe target), numeric evaluation, live interactive editing (cursor, backspace, fractions/roots/exponents), function/parametric/polar graphing sampling, and CAS (symbolic simplify/derivative, with the option of the vendored Eigenmath engine for real symbolic math) — plus fixing the current design's actual weaknesses (re-parses every frame with no caching, hard-coupled to this app's grayscale-only convention, and transitively drags the Vulkan font engine into any consumer that touches the layout code at all).

Scope decisions already confirmed with the user:
- **Eigenmath** (19k-line vendored CAS): include it, but behind a CMake option, **off by default** — matches the original app's own `#ifdef CAS_USE_EIGENMATH` gate.
- **Interactive editor**: port it **and** wire it into a real, type/backspace/arrow-key-able demo on the Math page (not just unit-tested).
- **Persistence** (sqlite3-backed workspace storage): **out of scope**.
- **Graphing/CAS visible demo pages**: out of scope for this round (library + unit tests only) — the user's near-term ask is a complete *library*; a graph-view/CAS-result page is a natural next step for whoever builds the Desmos/GeoGebra-style app on top of it later, not required now.

## Architecture

### New library: `libs/mathcore/`
Mirrors the existing `libs/firstparty/vulkan_font_engine/core/CMakeLists.txt` pattern already in this repo (`add_library(... STATIC ...)`, `target_include_directories(... PUBLIC ...)`, consumed via `add_subdirectory` + `target_link_libraries`). Not a submodule (first-party-authored, ported by the same author) — lives in `libs/mathcore/` alongside `libs/firstparty/`, with its own `include/mathcore/*.h` (public, nested-folder includes so consumer apps never collide on bare names like `parser.h`) and `src/*.cpp`.

Public headers: `ast.h`, `lexer.h`, `parser.h` (relocated as-is), `ink.h` (new), `imath_canvas.h` / `imath_font_metrics.h` (new — the rendering-backend seam), `mathbox.h`, `math_layout.h` (ported, now interface-based), `evaluator.h` / `numeric_evaluator.h` (new), `cached_expression.h` (new), `editor.h` / `editor_layout.h` / `calc.h` (new), `graphing/equation.h` / `graphing/plot.h` (new), `cas/cas.h` / `cas/symbolic_engine.h` / `cas/eigenmath_engine.h` (new, latter gated).

`windows_ui_demo`'s `src/math/` shrinks to just the two concrete adapters: `math_canvas.h/.cpp` (implements `mathcore::IMathCanvas`) and `msdf_font_metrics_adapter.h/.cpp` (implements `mathcore::IMathFontMetrics` over the vendored `MsdfFont`). Everything else moves into `libs/mathcore`.

### Breaking the Vulkan-font-engine dependency: two interfaces, not one
`mathbox.cpp` calls a real, wide surface of `MsdfFont` methods (`hasMath`, `mathConstants`, `glyphByKey`, `glyphPadEm`, `mathKey`, `construction`, `buildVStretch`, `advanceKey`, `keyForStyle`) plus several vendored value types (`MathConstants`, `MsdfGlyph`, `MathConstruction`, `VStretch`). Folding all of that into `IMathCanvas` would bloat one interface with font-specific concerns; instead:

```cpp
// mathcore/imath_font_metrics.h
namespace mathcore {
enum class FontStyle : uint8_t { Roman, Bold, Italic };
struct MathConstants { float axisHeight, scriptPercentScaleDown, scriptScriptPercentScaleDown,
                        spaceAfterScript, /* + every other accessor mathbox.cpp actually reads —
                        enumerate exactly from real call sites when writing this file, do not guess */; };
struct GlyphMetrics { float planeT=0, planeB=0, planeL=0, planeR=0; };
struct MathConstruction { /* opaque-ish; only what construction()/buildVStretch() need */ };
struct PlacedPart { uint32_t key; float bottomEm; };
struct VStretch { bool single=false; uint32_t key=0; float heightEm=0; std::vector<PlacedPart> parts; };

class IMathFontMetrics {
public:
    virtual ~IMathFontMetrics() = default;
    virtual bool hasMath() const = 0;
    virtual const MathConstants& mathConstants() const = 0;
    virtual float glyphPadEm() const = 0;
    virtual uint32_t mathKey(uint32_t codepoint) const = 0;
    virtual uint32_t keyForStyle(FontStyle, uint32_t codepoint) const = 0;
    virtual const GlyphMetrics* glyphByKey(uint32_t key) const = 0;         // nullable
    virtual const MathConstruction* construction(uint32_t key) const = 0;  // nullable
    virtual VStretch buildVStretch(const MathConstruction&, float targetEm) const = 0;
    virtual float advanceKey(uint32_t key, float sizePx) const = 0;
};
}

// mathcore/imath_canvas.h
namespace mathcore {
class IMathCanvas {
public:
    virtual ~IMathCanvas() = default;
    virtual const IMathFontMetrics* mathFontMetrics() const = 0;   // null when no math font loaded
    virtual void  text(std::string_view s, float x, float y, float size, Ink ink) = 0;
    virtual float textWidth(std::string_view s, float size) const = 0;
    virtual void  rect(float x, float y, float w, float h, Ink ink) = 0;
    virtual void  mathGlyph(uint32_t key, float x, float y, float size, Ink ink) = 0;
};
}
```
`mathbox.h`/`math_layout.h`/the new editor/graphing code depend only on these two interfaces — zero knowledge of `MsdfFont` or Vulkan anywhere in `libs/mathcore`. `windows_ui_demo`'s `MsdfFontMetricsAdapter` wraps the real `MsdfFont`, converting its real types into these POD mirrors (cheap, once per layout pass — not perf-sensitive). A future non-Vulkan host (or a unit test) can implement both interfaces with stub/fake data and link zero FreeType/msdfgen/Vulkan.

### Color: one `Ink` POD, not a template, not a second parameter
```cpp
// mathcore/ink.h
struct Ink {
    float r=0.9f, g=0.9f, b=0.9f, a=1.0f;    // default matches every current 0.9f gray default
    static constexpr Ink gray(float v, float alpha=1.0f) { return {v,v,v,alpha}; }
};
```
Replaces every `float gray` parameter in `mathbox`/`math_canvas`/`math_layout` 1:1 (same arity — `Ink::gray(0.9f)` is a mechanical substitution at every windows_ui_demo call site). Rejected alternatives: templating the layout functions (can't mix with the virtual `IMathCanvas` interface; would force a header-only rewrite of ~700 lines), and a second optional-tint parameter (adds branching at every one of the ~40 call sites for no benefit over a single POD).

### Performance: `CachedExpression`, living inside the library
```cpp
// mathcore/cached_expression.h
class CachedExpression {
public:
    bool setSource(std::string expr);                                    // false on parse error
    AstFit fit(IMathCanvas& c, float maxSize, float maxWidth);            // re-tokenizes/re-parses only if source changed; re-lays-out only if source/size/maxWidth changed
    void draw(IMathCanvas& c, float rightX, float yAxis, Ink ink) const;  // always cheap — walks the cached tree
private:
    std::string source_; mathx::NodePtr ast_; mb::NodePtr laidOut_;
    float lastSize_=-1, lastMaxWidth_=-1;
};
```
Opt-in wrapper (raw `tokenize`/`parse`/`fitAst`/`drawAstAt` remain directly callable) — but it's the one thing every consumer needs (this app's static examples, a future grapher's live-typed formula, a future calculator's result tape), so it's solved once in the library rather than reinvented per host. Fixes the current `page_math.cpp` re-tokenizing+re-parsing+re-laying-out 4 hardcoded strings from scratch every single frame.

Graphing's `sampleEquation` gets the equivalent fix directly in its own port: parse the expression **once** before the sample loop, then rebind `EvalContext::vars` and call `NumericEvaluator::eval` per sample — not the original's per-sample re-tokenize+re-parse (a `TODO(perf, M7)` left unresolved in the source app; fixed here as part of the port, not copied verbatim).

### Caret rendering — confirmed against the real original source
The original `mathbox.cc`'s `Node::Caret` draw case is a no-op (`return; // drawn by the parent HBox`) — the caret is actually drawn by its **parent HBox**, via a real helper:
```cpp
void drawCaret(Canvas& c, float x, float top, float bot) {
    c.rect(x, top, std::max(1.5f, (bot - top) * 0.04f), bot - top, col::accent);
}
```
called at each `Node::Caret` child position inside `HBox`'s draw loop (only `if (s.caret)`), plus a second call site in the `Node::Script` case for `n.scriptCaret` (caret sitting between a base and its exponent while mid-typed). This app's current `mathbox.h::draw()` dropped the old `Style{caret}` flag entirely when it collapsed `Style`→`float gray`. Port faithfully: add a `bool showCaret` parameter to `draw()` (threaded through recursive calls, root-call default `false`), reproduce `drawCaret` exactly (same 1.5px-min / 4%-of-height width formula) using `IMathCanvas::rect()`, called from HBox's loop and from the Script case — not invented from scratch.

### Keyboard input — confirmed genuinely missing
Grepped `src/platform/` for `WM_CHAR`/`WM_KEYDOWN`/`WM_KEYUP` — no matches. This app currently has zero keyboard text-entry support (only mouse via `InputState`). The live-editor demo task must add real `WM_CHAR`/`WM_KEYDOWN` handling to `Window`'s message pump and `InputState` (arrow keys + backspace need `WM_KEYDOWN` virtual-key codes; character insertion needs `WM_CHAR`) — this is new platform-layer work, not just plumbing an existing input source into a new page. Sized as its own step within the demo task.

### Eigenmath opt-in build
`build.bat` currently only parses `debug`/`release`/`clean` — no passthrough for extra CMake defines. Add a new recognized arg (`eigenmath`) that sets `-DMATHCORE_ENABLE_EIGENMATH=ON` on the `cmake -G Ninja -B ...` configure line, so `build.bat debug eigenmath` builds with it and plain `build.bat debug` doesn't — keeps the "never invoke cmake/ninja outside build.bat's vcvars environment" rule intact while making the opt-in build reachable at all.

## Global Constraints

- Build exclusively via `build.bat` (CMake+Ninja+MSVC) — never invoke `cmake`/`ninja` directly outside its vcvars environment.
- New library lives at `libs/mathcore/` (not a submodule — first-party-authored/ported, mirrors `libs/firstparty/vulkan_font_engine/core/CMakeLists.txt`'s `add_library(... STATIC ...)` + `target_include_directories(... PUBLIC ...)` pattern). Public headers under `libs/mathcore/include/mathcore/*.h` (nested-folder includes, e.g. `#include <mathcore/parser.h>` — never bare `#include "parser.h"`).
- `windows_ui_demo`'s `src/math/` shrinks to exactly two concrete adapter files: `math_canvas.h/.cpp` (implements `mathcore::IMathCanvas`) and `msdf_font_metrics_adapter.h/.cpp` (implements `mathcore::IMathFontMetrics`). Everything else (`ast`, `lexer`, `parser`, `mathbox`, `math_layout`, and all new components) lives in `libs/mathcore`.
- Grayscale-only convention for windows_ui_demo itself is preserved via `Ink::gray(v)` — mathcore's own public API takes full `Ink{r,g,b,a}` (see the `Ink` POD below), never a raw `float gray`.
- Every newly-ported file carries this repo's existing provenance-comment convention (see `mathbox.h`/`math_layout.h`/`ast.h` today): a header comment naming the exact source path under `C:\Users\incxiuefb\Documents\Files\clone\calculator` and reproducing that file's AGPLv3 copyright line. Exception: `libs/mathcore/third_party/eigenmath/` is BSD-2-Clause (George Weigt) — keep its own `LICENSE-eigenmath.txt`, and keep `eigenmath_engine.cc`'s header comment explicit that the *bridge* file is AGPLv3 (ported from calculator's own `eigenmath_engine.cc`) while the *vendored engine* is BSD-2-Clause — never blur the two into one license statement.
- Unit tests: plain `assert()`-based standalone executables, matching the existing `parser_test` pattern exactly — one test binary per new component, declared in `libs/mathcore/CMakeLists.txt`, run directly as `.\build_debug\<name>_test.exe` after `build.bat debug`. No test framework.
- `Ink` POD (`libs/mathcore/include/mathcore/ink.h`):
  ```cpp
  struct Ink {
      float r=0.9f, g=0.9f, b=0.9f, a=1.0f;
      static constexpr Ink gray(float v, float alpha=1.0f) { return {v,v,v,alpha}; }
  };
  ```
- `IMathFontMetrics` / `IMathCanvas` interface shapes (exact signatures — field lists inside `MathConstants`/`MathConstruction`/`VStretch` must be enumerated from real call sites in Task 2, not guessed):
  ```cpp
  namespace mathcore {
  enum class FontStyle : uint8_t { Roman, Bold, Italic };
  struct GlyphMetrics { float planeT=0, planeB=0, planeL=0, planeR=0; };
  struct PlacedPart { uint32_t key; float bottomEm; };
  struct VStretch { bool single=false; uint32_t key=0; float heightEm=0; std::vector<PlacedPart> parts; };
  class IMathFontMetrics {
  public:
      virtual ~IMathFontMetrics() = default;
      virtual bool hasMath() const = 0;
      virtual const MathConstants& mathConstants() const = 0;
      virtual float glyphPadEm() const = 0;
      virtual uint32_t mathKey(uint32_t codepoint) const = 0;
      virtual uint32_t keyForStyle(FontStyle, uint32_t codepoint) const = 0;
      virtual const GlyphMetrics* glyphByKey(uint32_t key) const = 0;
      virtual const MathConstruction* construction(uint32_t key) const = 0;
      virtual VStretch buildVStretch(const MathConstruction&, float targetEm) const = 0;
      virtual float advanceKey(uint32_t key, float sizePx) const = 0;
  };
  class IMathCanvas {
  public:
      virtual ~IMathCanvas() = default;
      virtual const IMathFontMetrics* mathFontMetrics() const = 0;
      virtual void  text(std::string_view s, float x, float y, float size, Ink ink) = 0;
      virtual float textWidth(std::string_view s, float size) const = 0;
      virtual void  rect(float x, float y, float w, float h, Ink ink) = 0;
      virtual void  mathGlyph(uint32_t key, float x, float y, float size, Ink ink) = 0;
  };
  }
  ```
- Caret rendering formula (confirmed against the real original `mathbox.cc`, not invented): `drawCaret(c, x, top, bot)` draws `c.rect(x, top, std::max(1.5f, (bot-top)*0.04f), bot-top, <caret ink>)`, called from `HBox`'s draw loop at each `Node::Caret` child (only when a `showCaret` flag is on) and from the `Script` case when `n.scriptCaret` is set. `mathbox::draw()` needs a `bool showCaret` parameter (root-level default `false`, threaded through recursive calls) to gate this — it does not exist today.
- `build.bat` gains one new recognized arg, `eigenmath`, which adds `-DMATHCORE_ENABLE_EIGENMATH=ON` to its `cmake -G Ninja -B ...` configure line (plain `build.bat debug`/`build.bat release` leave it OFF/undefined).
- This is a refactor-and-extend: the existing Math page's 4 static example equations must keep rendering exactly as they do today (pixel-identical) after the restructure — never a visual regression.

## Task list (dependency order)

### Task 1: Scaffold `libs/mathcore/`, relocate `ast.h`/`lexer.*`/`parser.*`

New `libs/mathcore/CMakeLists.txt` (`add_library(mathcore STATIC ...)`, `target_include_directories(mathcore PUBLIC include)`, `target_compile_features(mathcore PUBLIC cxx_std_17)`), headers moved to `libs/mathcore/include/mathcore/{ast,lexer,parser}.h`, sources to `libs/mathcore/src/{lexer,parser}.cpp`, `libs/mathcore/test/parser_test.cpp` (moved from `src/math/parser_test.cpp`, includes updated to `<mathcore/parser.h>` etc., declared as its own test executable inside `libs/mathcore/CMakeLists.txt`).

Delete `src/math/ast.h`, `src/math/lexer.h`, `src/math/lexer.cpp`, `src/math/parser.h`, `src/math/parser.cpp`, `src/math/parser_test.cpp`. In root `CMakeLists.txt`: remove the old `parser_test` target and the `src/math/lexer.cpp`/`src/math/parser.cpp` lines from `windows_ui_demo`'s `target_sources`; add `add_subdirectory(libs/mathcore)` and link `mathcore` into `windows_ui_demo` via `target_link_libraries`.

`src/math/mathbox.h/.cpp` and `src/math/math_layout.h/.cpp` still exist at this point and still `#include "ast.h"`/`"parser.h"` by relative path — since those files move to `libs/mathcore` in Tasks 3-4, for this task only, update their includes to `#include <mathcore/ast.h>` / `#include <mathcore/parser.h>` (via the new `mathcore` link) so they keep compiling, OR (implementer's choice, whichever is less churn) leave `src/math/mathbox.*`/`math_layout.*` temporarily broken and skip building `windows_ui_demo` this task — **the required verification for this task is `libs/mathcore`'s own `parser_test.exe` only**; full-app build verification is deferred to Task 5 where mathbox/math_layout land in their final form. State clearly in the task report which choice was made.

**Verify:** `build.bat debug` (or `cmake --build build_debug --target parser_test` if the full app doesn't build yet — note which), run `.\build_debug\parser_test.exe` (or wherever it lands), confirm identical pass/fail output to before the move.

**Commit:** "Relocate math AST/lexer/parser into libs/mathcore static library"

### Task 2: Define `Ink`, `IMathCanvas`, `IMathFontMetrics`

New `libs/mathcore/include/mathcore/ink.h`, `imath_canvas.h`, `imath_font_metrics.h` — exact shapes given in Global Constraints above, except `MathConstants`'s field list, which must be enumerated for real: read every `mf->...`/`c.msdfFont()->...` call site in the *current* `src/math/mathbox.cpp` (there are roughly 40) and cross-reference against the real accessor names in `libs/firstparty/vulkan_font_engine/core/msdf.hh`'s `MathConstants` class — write down every accessor mathbox.cpp actually calls (not the full upstream set) as a same-named method on the mirror `MathConstants` struct. Do the same cross-reference for what fields `MsdfGlyph`/`MathConstruction` need to become `GlyphMetrics`/the opaque `MathConstruction` mirror.

**Verify:** headers compile standalone — write a throwaway `.cpp` in `libs/mathcore/test/` that `#include`s all three and instantiates nothing (a `-fsyntax-only`-equivalent check via normal compilation of a TU with no `main` symbol conflicts, or add a temporary object-only CMake check), confirm it compiles with no errors, then remove the throwaway file (or fold it into Task 3's fake-interface harness so it isn't wasted).

**Commit:** "Define IMathCanvas/IMathFontMetrics rendering-backend seam and Ink color type"

### Task 3: Port `mathbox` onto the new interfaces

New `libs/mathcore/include/mathcore/mathbox.h`, `libs/mathcore/src/mathbox.cpp` — start from this app's *current* `src/math/mathbox.h/.cpp` (already grayscale-adapted from the original calculator source; do not re-derive from the calculator repo directly) and mechanically adapt: `MathCanvas&` parameters → `IMathCanvas&`, every `float gray` → `Ink ink`, every direct `MsdfFont`/`c.msdfFont()` call → `c.mathFontMetrics()`/`IMathFontMetrics` mirror-type call (using the exact accessor names fixed in Task 2), re-add `bool showCaret` to `draw()` per the Global Constraints caret formula (implement `drawCaret` faithfully — the 1.5px-min/4%-height-width bar — call it from `HBox`'s draw loop at `Node::Caret` children and from the `Script` case when `n.scriptCaret` is set). This is the largest single task in the plan — budget accordingly, and read every call site rather than pattern-matching blindly, since field names must match Task 2's mirror types exactly.

Also create a throwaway fake `IMathCanvas`/`IMathFontMetrics` implementation in `libs/mathcore/test/` (fixed/dummy metrics, no real font) purely to compile+link `mathbox.cpp` standalone and catch signature mismatches — this is not a real functional test (no real math-typesetting behavior to assert yet), just a compile/link smoke check; keep it around for Task 4 and Task 11 to reuse.

**Verify:** the fake-interface harness compiles and links (`libs/mathcore` builds as a static library with `mathbox.cpp` included, and a throwaway test `.cpp` linking against the fake implementations succeeds).

**Commit:** "Port mathbox to IMathCanvas/IMathFontMetrics/Ink"

### Task 4: Port `math_layout` (AST-half) onto the new interfaces

New `libs/mathcore/include/mathcore/math_layout.h`, `libs/mathcore/src/math_layout.cpp` — same mechanical adaptation as Task 3 (start from the current `src/math/math_layout.h/.cpp`), smaller (already trimmed to the AST-only half — `fitAst`/`drawAstAt`).

**Verify:** compiles+links against the Task 3 fake-interface harness.

**Commit:** "Port math_layout (fitAst/drawAstAt) to IMathCanvas/Ink"

### Task 5: Concrete windows_ui_demo adapters

New `src/math/msdf_font_metrics_adapter.h/.cpp` (`class MsdfFontMetricsAdapter : public mathcore::IMathFontMetrics`, wraps a `const MsdfFont&`, every method a thin forwarding call converting the vendored engine's real types into the `libs/mathcore` mirror types from Task 2). Rewrite `src/math/math_canvas.h` (`class MathCanvas : public mathcore::IMathCanvas`, same responsibilities as today but `Ink` instead of `float gray`, owns an `MsdfFontMetricsAdapter` member constructed once `text_.hasMathFont()` is true, `mathFontMetrics()` returns `&adapter_` or `nullptr`). Delete `src/math/mathbox.h/.cpp` and `src/math/math_layout.h/.cpp` (now living in `libs/mathcore`, per Tasks 3-4). Update root `CMakeLists.txt`: remove `src/math/mathbox.cpp`/`src/math/math_layout.cpp` from `windows_ui_demo`'s `target_sources`; keep `src/math/math_canvas.cpp` and add `src/math/msdf_font_metrics_adapter.cpp`. Update `src/app/page_math.cpp`'s includes to `<mathcore/...>` paths.

**Verify:** `build.bat debug`, run the app, navigate to the Math tab, screenshot, and confirm the 4 existing example equations render **pixel-identically** to a screenshot taken before this task's changes (this is the regression bar for the entire restructure to this point — a pure architecture change with zero intended visual difference). Attach or describe both screenshots in the task report.

**Commit:** "Wire windows_ui_demo's MathCanvas through the new IMathCanvas/IMathFontMetrics seam"

### Task 6: `Ink` rollout in `page_math.cpp`

In `src/app/page_math.cpp`, change the `drawAstAt(..., 0.9f)` call to `drawAstAt(..., mathcore::Ink::gray(0.9f))` (or wherever the literal `0.9f` gray value is threaded through). Grep the whole `src/app/` and `src/math/` tree for any remaining raw `float gray`/bare float literal passed where mathcore's public API now expects an `Ink` — confirm none remain.

**Verify:** `build.bat debug`, screenshot the Math page, confirm visually identical to Task 5's baseline.

**Commit:** "Switch windows_ui_demo's math page to Ink-based draw calls"

### Task 7: Port `IEvaluator`/`EvalContext`/`EvalResult`/`NumericEvaluator`

New `libs/mathcore/include/mathcore/evaluator.h` (interface: `EvalContext{bool degrees, double ans, const std::vector<std::pair<std::string,double>>* vars, bool lookupVar(name, out) const}`, `EvalResult{bool ok, double value, std::string error}`, `class IEvaluator { virtual EvalResult eval(const Node&, const EvalContext&) const = 0; }`) and `numeric_evaluator.h` + `libs/mathcore/src/numeric_evaluator.cpp` (near-verbatim port of `calculator/core/math/numeric_evaluator.cc` — recursive-descent eval over all 10 `mathx::Node::Kind`s, `Call` handling sin/cos/tan/sqrt/ln/log, div-by-zero → `EvalResult{ok=false, error="Undefined"}`, domain errors → `"Math Error"`, unbound `Var` → `"Undefined"`, degrees/radians mode, trig near-zero snapping). Confirmed dependency-free (only `<cmath>` + `ast.h`) — port as-is, no interface adaptation needed.

`libs/mathcore/test/evaluator_test.cpp`: assert-based coverage of arithmetic (`2+2==4` etc.), each `Call` function (sin/cos/tan/sqrt/ln/log) against a known value, division by zero returns `ok=false`, `sqrt`/`ln`/`log` of an out-of-domain argument returns `ok=false`, an unbound `Var` returns `ok=false`, degrees vs. radians mode changes trig results as expected, and the near-zero trig snapping behavior (e.g. `sin(pi)` in radians should read as `0`, not a tiny epsilon).

**Verify:** `evaluator_test.exe` passes (all assertions).

**Commit:** "Port numeric evaluator (IEvaluator/NumericEvaluator) with unit tests"

### Task 8: Wire evaluation into `page_math.cpp`

In `src/app/page_math.cpp`, construct a `mathcore::NumericEvaluator`, build an `EvalContext` (degrees=false, no vars needed since the 4 examples are closed-form), evaluate each example's already-parsed `mathx::Node` root, and draw a small "= <value>" text line beneath each typeset equation (via the existing `MathCanvas::text`/`TextRenderer::drawText` path) — format the double with a reasonable fixed precision (e.g. 3-4 significant decimal places).

**Verify:** `build.bat debug`, run the app, confirm all 4 examples show a numerically correct result line. Hand-compute expected values first: `1/(2+sqrt(3)) ≈ 0.2679`, `x^2+2*x+1` — no bound `x`, so this one should show its `EvalResult.ok=false`/"Undefined" state rather than a bogus number (call this out explicitly in the report, since it's the one example with a free variable), `sqrt(2)/2 ≈ 0.7071`, `(1+2)*(3-4)/5 = -0.6`. Screenshot.

**Commit:** "Show computed numeric values on the Math page example equations"

### Task 9: `CachedExpression` + wire it into `page_math.cpp`

New `libs/mathcore/include/mathcore/cached_expression.h` + `libs/mathcore/src/cached_expression.cpp` — exact shape given in the plan's Architecture section above (`setSource`/`fit`/`draw`, re-tokenizes/re-parses only on source change, re-lays-out only on source/size/maxWidth change). `libs/mathcore/test/cached_expression_test.cpp`: assert that calling `fit()` twice with an unchanged source/size/maxWidth does not re-parse (e.g. expose the underlying AST pointer for test purposes and assert pointer identity is unchanged across the two calls; or inject a call counter — implementer's choice, document which), and that changing the source string does trigger a re-parse (pointer changes / counter increments).

Rewrite `src/app/page_math.cpp` to hold 4 `static mathcore::CachedExpression` instances (one per example) instead of calling `tokenize`/`parse`/`fitAst` fresh every frame — `setSource()` called once (e.g. on first frame or via static initialization), `fit()`/`draw()` called every frame (cheap, cache-hit path).

**Verify:** `cached_expression_test.exe` passes; `build.bat debug`, run the app, confirm visually identical output to Task 8's baseline (screenshot).

**Commit:** "Add CachedExpression and use it for the Math page's static examples"

### Task 10: Port `calcedit::Editor`/`Row`/`Node`/`Cursor`

New `libs/mathcore/include/mathcore/editor.h` + `libs/mathcore/src/editor.cpp` — port `calculator/core/calc/editor.hh/.cc` faithfully (confirmed pure, portable C++17, no Android/JNI dependency). Recommend nesting the ported `calcedit` namespace inside `mathcore` (i.e. `namespace mathcore { namespace calcedit { ... } }`) for minimal-diff porting against the original source. Preserve exactly: `Row{items, parentNode, slotIndex}`, `Node{kind:Atom/Frac/Sqrt/NthRoot/Power, text, slots, parentRow}`, `Cursor{row,index}`, `class Editor` with `clear/loadString/insertAtom/insertLiteral/insertFraction/insertSqrt/insertNthRoot/insertPower/backspace/moveLeft/moveRight/moveUp/moveDown/linearize/root/cursor/empty/isLoneZero/cursorAtRoot`. Port the backspace state machine (delete-atom / delete-empty-template / step-into-template-from-right-edge / cross-sibling-slot / unwrap-template-at-slot-start) and navigation (entrySlotFromLeft/Right, rightSiblingSlot/leftSiblingSlot, Frac-only moveUp/moveDown) exactly as in the original — this is a real state machine, port it faithfully rather than reimplementing from the plan's summary.

`libs/mathcore/test/editor_test.cpp`: cover `insertFraction`/`insertSqrt`/`insertNthRoot`/`insertPower` each producing the right tree shape and cursor position, `insertPower`'s implicit-base-requires-a-value-ending-left-sibling guard (fails at row start / right after an operator / right after `(`), backspace unwrapping a template at slot start (e.g. `8/5` backspaced at the start of "8" → `85`), backspace deleting an empty template in one press, cross-slot navigation (`moveRight` out of a numerator into the denominator, `moveUp`/`moveDown` between Frac's num/den), and `linearize()` round-trips (`Frac`→`"(num)/(den)"`, `Sqrt`→`"sqrt(rad)"`, `NthRoot`→`"(radicand)^(1/(index))"`, `Power`→`"^(exp)"` appended after the preceding emission).

**Verify:** `editor_test.exe` passes.

**Commit:** "Port calcedit::Editor (structured 2D input tree) with unit tests"

### Task 11: Re-add caret support to `mathbox::draw()`; port `buildEdRow`/`drawEditor`

Add the `bool showCaret` parameter to `libs/mathcore/include/mathcore/mathbox.h`'s `draw()` (and thread it through recursive calls) plus the `drawCaret` helper and its two call sites (HBox loop, Script's `scriptCaret` case) — per the Global Constraints caret formula, confirmed against the real original source, not invented. New `libs/mathcore/include/mathcore/editor_layout.h` + `libs/mathcore/src/editor_layout.cpp` — port `buildEdRow` (builds an `mb::Node` HBox from a live `Row`+`Cursor`, handles `parenLevels`-driven ghost/unmatched-delimiter rendering via `ghostClose`, ties `Frac`/`Sqrt`/`NthRoot`/`Power` editor nodes to `mb::Node::Frac`/`Radical`/`Script`, resolves `Power`'s implicit base via `isScriptBase()`, inserts `mb::Node::Caret` at the live cursor position) and `drawEditor` (iterative shrink-to-fit sizing + `mathbox::draw(..., showCaret=true)`), adapted from `IMathCanvas`/`Ink`/the Task 10 `Editor` — from `calculator/app/src/main/cpp/mathlayout.cc`'s `buildEdRow` (~lines 165-269) and `drawEditor` (~lines 435-459).

**Verify:** compiles+links against the Task 3 fake-interface harness (no live keyboard input wired yet — this task is render-path plumbing only, exercised for real in Task 13).

**Commit:** "Port editor-to-mathbox render bridge (buildEdRow/drawEditor), make caret real"

### Task 12: Port `Calc` facade

New `libs/mathcore/include/mathcore/calc.h` + `libs/mathcore/src/calc.cpp` — port the token-dispatch/facade *pattern* from `calculator/core/calc/calc.hh/.cc` (not a duplicate implementation — adapt it to mathcore's own `IEvaluator`/parser instead of hardcoding `NumericEvaluator` internally): `class Calc` takes a `const IEvaluator&` in its constructor, defaulting to an owned `NumericEvaluator` if none is supplied, owns a `calcedit::Editor`, and exposes `input(token)` dispatch (digits, operators, function names, template tokens frac/sqrt/nthroot/pow, navigation left/right/up/down, control C/back/=/degrad), `evaluate()`, `preview()`, ans-injection, auto-paren-balancing on evaluate — same behavior as the original, same public surface, minus the hardcoded evaluator dependency.

`libs/mathcore/test/calc_test.cpp`: token dispatch produces the right editor state, `evaluate()` on a well-formed expression returns the right numeric result, ans-injection after a trailing operator, auto-paren-balancing (e.g. `evaluate()` on `"(1+2"` behaves as if closed), `preview()` doesn't mutate state.

**Verify:** `calc_test.exe` passes.

**Commit:** "Port Calc facade (token dispatch, evaluate/preview) adapted to mathcore's evaluator"

### Task 13: Interactive live editor demo

Add real keyboard input to this app's platform layer (confirmed missing — grep found zero `WM_CHAR`/`WM_KEYDOWN`/`WM_KEYUP` handling anywhere under `src/platform/` today): extend `src/platform/window.h/.cpp`'s message pump to handle `WM_CHAR` (character insertion) and `WM_KEYDOWN` (arrow keys via `VK_LEFT`/`VK_RIGHT`/`VK_UP`/`VK_DOWN`, backspace via `VK_BACK`), extend `src/platform/input_state.h/.cpp` with whatever queryable state/event queue this app's existing `InputState` pattern favors (match its existing edge-detection style — check `mouseWentDown`/`mouseWentUp`'s pattern before choosing the keyboard equivalent's shape).

Extend `src/app/page_math.h/.cpp` with an editable-input-box mode: owns a `mathcore::calcedit::Editor` + `mathcore::Calc`, routes new keyboard events into `Calc::input(...)`/`Editor` methods (digits/operators/backspace/arrows; map a couple of convenient key combos or an on-screen button to `insertFraction`/`insertSqrt`/`insertPower` if a dedicated keystroke doesn't make sense — implementer's call, document the mapping chosen), calls `editor_layout::drawEditor(..., showCaret=blinkOn)` every frame, blinks the caret via the existing `anim/animated_float` infrastructure or a simple `fmod(time, 1.0) < 0.5` toggle.

**Verify:** `build.bat debug`, run the app, and interactively type a sequence exercising: plain digit/operator entry, `insertFraction` then typing a numerator and arrowing/tabbing into the denominator, `insertSqrt` then typing a radicand, backspacing a template from its slot-start (confirm it unwraps rather than vanishing outright), left/right arrow navigation across the whole expression. Screenshot the result and write down the exact key sequence typed and what was observed at each step in the task report — this is the one task in the whole plan that needs a real interaction transcript, not just a single before/after screenshot.

**Commit:** "Add interactive live math input editor to windows_ui_demo's Math page"

### Task 14: Port graphing (`Equation`/`sampleEquation`/`evalFunctionAt`)

New `libs/mathcore/include/mathcore/graphing/equation.h` (`enum class EqType{Function,Parametric,Polar}`, `struct RGBA{float r,g,b,a}`, `struct Equation{int id; EqType type; std::string expr,exprX,exprY; RGBA color; bool enabled; double tmin,tmax,tstep}`) and `graphing/plot.h` + `libs/mathcore/src/graphing/plot.cpp` (`struct SamplePt{double wx,wy; bool valid}`, `bool sampleEquation(const Equation&, double xmin, double xmax, int count, bool degrees, std::vector<SamplePt>& out)`, `bool evalFunctionAt(const std::string& expr, double x, bool degrees, double& outY)`) — ported from `calculator/core/graphing/equation.hh` + `plot.hh/.cc`, but with the perf fix described in the plan's Architecture section applied during the port (not copied verbatim): parse each expression **once** per `sampleEquation`/`evalFunctionAt` call (not once per sample inside the loop), then rebind a single-entry `EvalContext::vars` per sample and call `NumericEvaluator::eval` directly on the already-parsed AST. Function/Parametric/Polar dispatch logic (sweep `x` vs. sweep `t` and evaluate `exprX`/`exprY` vs. sweep angle and convert polar-to-Cartesian) ported as-is. `valid=false` marks pen-lifts for NaN/Inf/`EvalResult.ok=false`.

`libs/mathcore/test/plot_test.cpp`: sample `"x^2"` over a small range and assert each `SamplePt.wy` matches `x*x` within a tolerance; sample `"1/x"` across a range including `x=0` and assert `valid=false` exactly at/near the singularity and `valid=true` elsewhere; construct a `Parametric` and a `Polar` `Equation` and assert the dispatch produces geometrically sane output (e.g. a `Parametric` circle `exprX="cos(t)"`,`exprY="sin(t)"` over `t∈[0,2π]` produces points with `wx^2+wy^2≈1`).

**Verify:** `plot_test.exe` passes. No new windows_ui_demo page this task — library + tests only, per the scope decision in Context above.

**Commit:** "Port graphing/sampling (Equation/sampleEquation/evalFunctionAt) with parse-once optimization"

### Task 15: Port CAS interface + homegrown `symbolic_engine`

New `libs/mathcore/include/mathcore/cas/cas.h` (`enum class Op{Simplify,Numeric,Derivative,Integral}`, `struct Request{Op op; std::string expr,var="x"; bool degrees=false}`, `struct Reply{bool ok; std::string text,error}`, `class CasEngine{virtual Reply evaluate(const Request&, const std::atomic<bool>& cancel) = 0; virtual const char* name() const = 0}`) and `cas/symbolic_engine.h` + `libs/mathcore/src/cas/symbolic_engine.cpp` — ported from `calculator/core/cas/cas.hh` + `symbolic_engine.hh/.cc`: `class SymbolicEngine : public CasEngine` (`name()=="symbolic(dyno)"`), constant-folding + 0/1-identity `simplify()`, a `Differ` doing sum/product/quotient/power/chain-rule differentiation over sin/cos/tan/ln/log/sqrt, AST-to-infix-ASCII serialization, `Op::Numeric` delegating to the Task 7 `NumericEvaluator`, `Op::Integral` returning the original's documented `error = "needs a real CAS"`-style stub message (do not implement real integration — that's explicitly Eigenmath's job in Task 16).

`libs/mathcore/test/cas_test.cpp`: a few `Op::Simplify` constant-folding cases (e.g. `"1+1"` simplifies toward `"2"`, an identity like `"x*1"` or `"x+0"` simplifies away the no-op), a couple of `Op::Derivative` cases (`d/dx(x^2)` reduces to a form equivalent to `2*x`, a chain-rule case through `sin`/`sqrt`), confirm `Op::Integral` returns `ok=false` with the documented error text rather than crashing or silently no-oping.

**Verify:** `cas_test.exe` passes.

**Commit:** "Port CAS interface and homegrown symbolic_engine (simplify/derivative) backend"

### Task 16: Optional Eigenmath backend

Vendor `calculator/core/cas/eigenmath/eigenmath.c` (19,021 lines, single file, BSD-2-Clause, by George Weigt — verify whether the original splits out a header too and vendor that as well if so) into `libs/mathcore/third_party/eigenmath/`, with its own `LICENSE-eigenmath.txt` (verbatim BSD-2-Clause reproduction, separate from mathcore's own AGPLv3 notices per Global Constraints). New `libs/mathcore/include/mathcore/cas/eigenmath_engine.h` (`std::unique_ptr<CasEngine> makeEigenmathEngine();` — the only public symbol) + `libs/mathcore/src/cas/eigenmath_engine.cpp`, ported from `calculator/core/cas/eigenmath_engine.hh/.cc` (parses `req.expr` with mathcore's own lexer/parser into a `mathx::Node`, re-serializes into Eigenmath's own textual dialect, wraps in an Eigenmath command string (`d(expr,var)`/`integral(expr,var)`/`float(expr)`/`simplify(expr)`), calls Eigenmath's global `run(char*)` entry point, captures output via an injected hook into `Reply.text` — output side is pure string capture, no parse-back into `mathx::Node`; cooperative cancellation via a watcher thread flipping Eigenmath's global `interrupt` flag when the `CasEngine::evaluate` cancel token fires).

`libs/mathcore/CMakeLists.txt`: `option(MATHCORE_ENABLE_EIGENMATH "Vendor and build the Eigenmath CAS backend" OFF)`; when ON, add `src/cas/eigenmath_engine.cpp` + `third_party/eigenmath/eigenmath.c` to `target_sources(mathcore ...)` and `target_compile_definitions(mathcore PUBLIC MATHCORE_ENABLE_EIGENMATH)`. Guard the new files' inclusion in `libs/mathcore/test/cas_test.cpp` behind `#ifdef MATHCORE_ENABLE_EIGENMATH`.

`build.bat`: add a new recognized arg `eigenmath` (alongside the existing `clean`/`debug`/`release` parsing) that appends `-DMATHCORE_ENABLE_EIGENMATH=ON` to the `cmake -G Ninja -B ...` configure line.

**Verify:** twice. (1) Default build: `build.bat debug` (no `eigenmath` arg) — confirm the build succeeds, and confirm `eigenmath.c`/`eigenmath_engine.cpp` are NOT compiled (e.g. grep the Ninja build log for `eigenmath`, or check no corresponding `.obj` exists in `build_debug/`). (2) Opt-in build: `build.bat debug eigenmath` — confirm it successfully configures with the flag ON, compiles `eigenmath.c`, and the `#ifdef`-guarded CAS test case exercising the real Eigenmath backend passes.

**Commit:** "Add optional Eigenmath CAS backend (vendored, off by default)"

## Provenance
Every newly-ported file gets this repo's existing convention (already used in `mathbox.h`/`math_layout.h`/`ast.h`): a header comment naming the exact source path in `calculator` and reproducing its AGPLv3 copyright line. `third_party/eigenmath/` is the one exception — BSD-2-Clause, kept in its own `LICENSE-eigenmath.txt`, with `eigenmath_engine.cc`'s own header comment explicitly separating "this bridge file is AGPLv3, ported from calculator's `eigenmath_engine.cc`" from "the vendored engine itself is BSD-2-Clause, by George Weigt."

## Verification summary
- Parser/lexer/AST/evaluator/editor/plot/CAS: plain `assert()`-based test executables per component (matching the existing `parser_test` pattern), run directly after `build.bat debug`.
- mathbox/math_layout/CachedExpression/editor_layout: no automated rendering test (matches this repo's stated convention) — verified by running `windows_ui_demo` and screenshotting; `CachedExpression`'s caching behavior specifically is testable headlessly via the Task 3 fake-`IMathCanvas` harness.
- The `IMathCanvas`/`IMathFontMetrics` seam itself is proven by (a) fake-harness compile/link checks in Tasks 3-4 and (b) Task 5's pixel-identical-to-before regression check on the real app.
- Eigenmath gate verified both ways (off: zero cost added; on: backend actually works).
