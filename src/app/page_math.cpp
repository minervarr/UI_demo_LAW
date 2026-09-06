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

#include "layout.hh"
#include "keys.h"

// Example expressions exercising fractions, exponents, radicals (with a
// vinculum over the radicand), and nested parenthesized expressions.
static const char* kExamples[] = {
    "1/(2+sqrt(3))",
    "x^2+2*x+1",
    "sqrt(2)/2",
    "(1+2)*(3-4)/5",
};
constexpr int kExampleCount = static_cast<int>(sizeof(kExamples) / sizeof(kExamples[0]));

// Keyboard -> mathcore::calc::Calc token mapping for the live editor.
//
// Straight-through: every typed character that reaches FrameInput::typedChars
// (digits, '.', '+' '-' '*' '/' '^', '(' ')', '=', letters, ...) is forwarded
// verbatim as its own single-char token via Calc::input(). (One documented
// quirk: Calc::input special-cases the exact single-char token "C" as
// clear-all — typing a literal capital C clears the live expression.)
//
// The three 2D templates (frac/sqrt/pow) claim three otherwise unused
// printable keys:
//   '\\' (backslash)       -> insertFraction  (mnemonic: LaTeX "\frac")
//   '`'  (backtick/grave)  -> insertSqrt
//   '~'  (tilde)           -> insertPower
static void routeTypedChar(mathcore::calc::Calc& calc, char ch) {
    if (ch == '\\') { calc.input("frac"); return; }
    if (ch == '`')  { calc.input("sqrt"); return; }
    if (ch == '~')  { calc.input("pow");  return; }
    calc.input(std::string(1, ch));
}

void MathPage::update(float dtSeconds, const FrameInput& input, Rect area) {
    blinkClock_ = std::fmod(blinkClock_ + dtSeconds, 1.0f);

    for (char ch : input.typedChars) routeTypedChar(liveCalc_, ch);
    if (input.keyWentDown(keys::Back))  liveCalc_.input("back");
    if (input.keyWentDown(keys::Left))  liveCalc_.input("left");
    if (input.keyWentDown(keys::Right)) liveCalc_.input("right");
    if (input.keyWentDown(keys::Up))    liveCalc_.input("up");
    if (input.keyWentDown(keys::Down))  liveCalc_.input("down");


}

void MathPage::draw(MathCanvas& canvas, Rect area, float uiScaleFactor) {
    // One CachedExpression per static example, reused across frames instead
    // of tokenizing/parsing/laying out from scratch every frame: setSource()
    // with the same literal string is a cheap no-op after the first call.
    static mathcore::CachedExpression exprs[kExampleCount];

    // Scrollable + clipped: the stacked examples plus the live editor panel
    // exceed the window height once the content scale grows.
    canvas.setClip(area.x, area.y, area.w, area.h);

    float y = area.y + 60.0f * uiScaleFactor;
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

        // fit.above/fit.below bound the full drawn ink; inkBottom is the TRUE
        // bottom of the drawn equation and anchors the result line's gap.
        // rowHeight's *1.2 floor is pure row-advance padding, NOT part of the
        // ink — anchoring the result line to it made the visible gap balloon
        // whenever an expression's extents fell short of the floor.
        float inkBottom = yAxis + fit.below;
        float rowHeight = std::max(fit.above + fit.below, fit.size * 1.2f);

        // Evaluate the same already-parsed AST (no second tokenize/parse) and
        // draw a small "= <value>" line beneath the typeset equation.
        // Expressions with unbound free variables come back ok=false; show
        // the evaluator's own error string so the failure is visible.
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
        // bottom and the answer's ink TOP: MathCanvas::text takes a baseline,
        // and the answer's ascender reaches ~0.8*resultSize above it.
        float resultGap = resultSize * 0.5f;
        float resultY = inkBottom + resultGap + resultSize * 0.8f;
        canvas.text(buf, area.x + 40.0f * uiScaleFactor, resultY, resultSize,
                    mathcore::Ink::gray(0.6f));

        y += rowHeight + resultGap + resultSize * 1.3f + gap;
    }

    // Live, type/backspace/arrow-key-able expression, additive to the static
    // examples above. Rendered every frame through editor_layout::drawEditor
    // with a blinking caret.
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
        // A result is being shown (user pressed '='); draw the pretty result
        // string in the same panel instead of the frozen editor tree.
        canvas.text(liveCalc_.displayText(),
                    panelX + panelW - 40.0f * uiScaleFactor - 200.0f * uiScaleFactor,
                    y + panelH * 0.65f, size * 0.5f, mathcore::Ink::gray(0.9f));
    }
    y += panelH + 10.0f * uiScaleFactor;

    std::string preview = liveCalc_.preview();
    if (!preview.empty()) {
        std::string previewBuf = "= " + preview;
        // MathCanvas::text takes a BASELINE; drop it by the ascent so the
        // preview's ink starts below the panel instead of overlapping it.
        canvas.text(previewBuf, panelX, y + resultSize * 0.8f, resultSize,
                    mathcore::Ink::gray(0.6f));
    }
    y += resultSize * 1.4f;

    // Measured by drawing: total unscrolled content height including bottom
    // pad, consumed by next frame's update() scroll clamp.
    contentWidth_  = area.w;
    contentHeight_ = y - area.y + 40.0f * uiScaleFactor;
    canvas.clearClip();
}
