#include "page_math.h"
#include <mathcore/cached_expression.h>
#include <mathcore/editor_layout.h>
#include <mathcore/ink.h>
#include <mathcore/math_layout.h>
#include <mathcore/numeric_evaluator.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

// Example expressions chosen to exercise the features Tasks 7-9 actually
// support per the plan's design: fractions, exponents, radicals (with a
// vinculum over the radicand), and nested parenthesized expressions.
static const char* kExamples[] = {
    "1/(2+sqrt(3))",
    "x^2+2*x+1",
    "sqrt(2)/2",
    "(1+2)*(3-4)/5",
};
constexpr int kExampleCount = static_cast<int>(sizeof(kExamples) / sizeof(kExamples[0]));

// Keyboard -> mathcore::calc::Calc token mapping for the live editor (Task 13).
//
// Straight-through: every WM_CHAR character that reaches InputState::typedChars
// (digits, '.', '+' '-' '*' '/' '^', '(' ')', '=', letters, ...) is forwarded
// verbatim as its own single-char token via Calc::input(). Calc::appendToken's
// tokenText() passes anything that isn't one of "sin"/"cos"/"tan"/"ln"/"log"
// straight through to Editor::insertAtom(), so this covers plain digit/operator/
// paren/variable-letter entry with no extra plumbing, and '=' already matches
// Calc::input's own "=" -> evaluate() case. (One documented quirk: Calc::input
// also special-cases the exact single-char token "C" as clear-all -- typing a
// literal capital C, e.g. via Shift+C, clears the live expression instead of
// inserting the letter. Lowercase 'c' is unaffected. Left as-is rather than
// special-cased away, since it matches Calc's own existing keypad-token
// vocabulary and is called out here rather than silently surprising a reader.)
//
// The three 2D templates (frac/sqrt/pow) don't have an obvious single
// printable key (their natural keys -- '/', arrow-ish shapes, '^' -- are
// already claimed by flat operator entry), so this page claims three otherwise
// unused, easily-reached printable ASCII keys and documents the mapping here:
//   '\\' (backslash)      -> insertFraction  (mnemonic: LaTeX "\frac")
//   '`'  (backtick/grave)  -> insertSqrt
//   '~'  (tilde)            -> insertPower
// insertNthRoot has no key mapping in this pass (scope call, see task report).
static void routeTypedChar(mathcore::calc::Calc& calc, char ch) {
    if (ch == '\\') { calc.input("frac"); return; }
    if (ch == '`')  { calc.input("sqrt"); return; }
    if (ch == '~')  { calc.input("pow");  return; }
    calc.input(std::string(1, ch));
}

void MathPage::update(float dtSeconds, const InputState& input, Rect area) {
    blinkClock_ = std::fmod(blinkClock_ + dtSeconds, 1.0f);

    for (char ch : input.typedChars) routeTypedChar(liveCalc_, ch);
    if (input.keyBackspacePressed) liveCalc_.input("back");
    if (input.keyLeftPressed)      liveCalc_.input("left");
    if (input.keyRightPressed)     liveCalc_.input("right");
    if (input.keyUpPressed)        liveCalc_.input("up");
    if (input.keyDownPressed)      liveCalc_.input("down");

    const float kWheelStepPx = 60.0f;
    scrollY_ = clampScroll(scrollY_ - input.wheelDelta * kWheelStepPx,
                           contentHeight_, area.h);
}

void MathPage::draw(MathCanvas& canvas, Rect area, float uiScaleFactor) {
    // One CachedExpression per static example, reused across frames instead
    // of tokenizing/parsing/laying out from scratch every frame (Task 9):
    // setSource() is called every frame with the same literal string, which
    // is a cheap no-op after the first successful call (see
    // CachedExpression::setSource()'s own caching contract) -- the opt-in
    // caching lives inside mathcore, so this call site stays simple with no
    // caller-owned "did I already set this" guard.
    static mathcore::CachedExpression exprs[kExampleCount];

    // Scrollable + clipped: the stacked examples plus the live editor panel
    // exceed the window height once the content scale grows, so the page
    // scrolls via the mouse wheel and clips at the page edge.
    canvas.setClip(area.x, area.y, area.w, area.h);

    float y = area.y + 60.0f * uiScaleFactor - scrollY_;
    float size = 40.0f * uiScaleFactor;
    float maxWidth = area.w - 80.0f * uiScaleFactor;
    float gap = 40.0f * uiScaleFactor;
    float resultSize = size * 0.6f;

    mathx::NumericEvaluator evaluator;
    mathx::EvalContext ctx;
    ctx.degrees = false;  // radians; no vars bound, so "x^2+2*x+1" is expected
                           // to evaluate to ok=false ("Undefined") below.

    for (int i = 0; i < kExampleCount; ++i) {
        mathcore::CachedExpression& expr = exprs[i];
        if (!expr.setSource(kExamples[i])) continue;

        mathcore::mathlayout::AstFit fit = expr.fit(canvas, size, maxWidth);
        float rightX = area.x + 40.0f * uiScaleFactor + fit.width;
        float yAxis = y + fit.above;
        expr.draw(canvas, rightX, yAxis, mathcore::Ink::gray(0.9f));

        // fit.above/fit.below bound the full drawn ink (verified by
        // mathcore's fit_extents_test) now that the atlas bakes carry
        // bottom-anchored plane metadata — the old *2.2 safety floor
        // compensated for the per-glyph ceil-slack shift the bake used to
        // introduce, and inflated row advances so far that lower rows fell
        // off-screen. Keep only a small floor so an empty/degenerate fit
        // can't collapse the row.
        float rowHeight = std::max(fit.above + fit.below, fit.size * 1.2f);

        // Evaluate the same already-parsed AST (reusing CachedExpression's
        // cached tree via ast() -- no second tokenize/parse here) and draw a
        // small "= <value>" line beneath the typeset equation, proving the
        // eval path actually runs (not just the typesetter). Expressions
        // with unbound free variables (e.g. "x^2+2*x+1", where x has no
        // binding in ctx) are expected to come back ok=false; show the
        // evaluator's own error string ("Undefined") in that case rather
        // than a bogus number, so the failure is visible instead of silently
        // swallowed.
        char buf[64];
        const mathx::Node* astRoot = expr.ast();
        if (astRoot) {
            mathx::EvalResult result = evaluator.eval(*astRoot, ctx);
            if (result.ok) {
                std::snprintf(buf, sizeof(buf), "= %.4f", result.value);
            } else {
                std::snprintf(buf, sizeof(buf), "= %s",
                               result.error.empty() ? "Undefined" : result.error.c_str());
            }
        } else {
            std::snprintf(buf, sizeof(buf), "= Undefined");
        }
        // Proportional breathing room between the equation's measured ink
        // bottom (y + rowHeight, trustworthy per fit_extents_test) and the
        // answer's ink TOP: canvas.text takes a baseline, and the answer's
        // ascender reaches ~0.8*resultSize above it, so the baseline must
        // sit gap + ascent below the row bottom or tall ascenders ("= 0.7071")
        // poke back up into the equation.
        float resultGap = resultSize * 0.5f;
        float resultY = y + rowHeight + resultGap + resultSize * 0.8f;
        canvas.text(buf, area.x + 40.0f * uiScaleFactor, resultY, resultSize,
                    mathcore::Ink::gray(0.6f));

        y += rowHeight + resultGap + resultSize * 1.3f + gap;
    }

    // 5th row (Task 13): a live, type/backspace/arrow-key-able editable
    // expression, additive to the 4 static examples above (never replacing
    // them). Backed by mathcore::calc::Calc (owns its own
    // mathcore::calcedit::Editor), routed via update()'s keyboard handling and
    // rendered every frame through editor_layout::drawEditor with a blinking
    // caret (showCaret toggles on the fmod(time,1.0)<0.5 half of blinkClock_).
    float labelSize = resultSize;
    canvas.text("Live input -- type digits/operators; backslash=frac backtick=sqrt tilde=pow; backspace/arrows",
                area.x + 40.0f * uiScaleFactor, y, labelSize, mathcore::Ink::gray(0.5f));
    y += labelSize * 1.4f;

    float panelH = 90.0f * uiScaleFactor;
    float panelX = area.x + 40.0f * uiScaleFactor;
    float panelW = maxWidth;
    canvas.rect(panelX, y, panelW, panelH, mathcore::Ink::gray(0.25f, 0.4f));

    mathcore::Rect panel{panelX, y, panelW, panelH};
    bool caretOn = blinkClock_ < 0.5f;
    if (!liveCalc_.showingResult()) {
        mathcore::mathlayout::drawEditor(canvas, liveCalc_.editor(), panel, caretOn);
    } else {
        // A result is being shown (user pressed '='); the editor tree is
        // frozen behind it, so draw the pretty result string in the same
        // panel instead of the live tree (drawEditor always renders the
        // live editor -- there's no "show result" mode for it).
        canvas.text(liveCalc_.displayText(), panelX + panelW - 40.0f * uiScaleFactor - 200.0f * uiScaleFactor,
                    y + panelH * 0.65f, size * 0.5f, mathcore::Ink::gray(0.9f));
    }
    y += panelH + 10.0f * uiScaleFactor;

    std::string preview = liveCalc_.preview();
    if (!preview.empty()) {
        std::string previewBuf = "= " + preview;
        // canvas.text takes a BASELINE; drop it by the ascent so the
        // preview's ink starts below the panel instead of overlapping its
        // bottom border.
        canvas.text(previewBuf, panelX, y + resultSize * 0.8f, resultSize,
                    mathcore::Ink::gray(0.6f));
    }
    y += resultSize * 1.4f;

    // Measured by drawing: total unscrolled content height including bottom
    // pad, consumed by next frame's update() scroll clamp.
    contentHeight_ = (y + scrollY_) - area.y + 40.0f * uiScaleFactor;
    canvas.clearClip();
}
