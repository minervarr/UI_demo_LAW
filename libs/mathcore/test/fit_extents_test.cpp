// Functional extents test for mathlayout::fitAst / CachedExpression::fit:
// every visible mark a typeset expression draws (glyph ink, fraction bars,
// radical rules) must fall inside the vertical box the fit reports
// ([yAxis - fit.above, yAxis + fit.below]). windows_ui_demo's Math page
// positions each expression's "= result" line directly from fit.above/
// fit.below — when these under-measure (as they did with the top-anchored
// atlas plane metadata bug), the result text lands on top of the equation.
//
// Uses a recording subclass of the fake backend (fake_math_backend.h): the
// fake font reports every glyph with planeT=-0.62/planeB=0.02 and pad=0.02,
// so a glyph drawn with baseline y has true ink spanning
// [y - 0.60*size, y + 0.00*size] (plane ± pad).
#include <cassert>
#include <cstdio>

#include <mathcore/cached_expression.h>

#include "fake_math_backend.h"

using namespace mathcore;

namespace {

class RecordingCanvas : public test::FakeMathCanvas {
public:
    RecordingCanvas() : test::FakeMathCanvas(/*withFont=*/true) {}

    void text(std::string_view s, float, float baselineY, float size, Ink) override {
        if (s.empty()) return;
        noteInk(baselineY, size);
    }
    void mathGlyph(uint32_t, float, float baselineY, float size, Ink) override {
        noteInk(baselineY, size);
    }
    void rect(float, float y, float, float h, Ink) override {
        noteSpan(y, y + h);
    }

    void reset() { minY = 1e9f; maxY = -1e9f; }
    float minY = 1e9f, maxY = -1e9f;

private:
    // Fake-font glyph ink about the baseline: above = -(planeT + pad) = 0.60,
    // below = planeB - pad = 0.00 (in em).
    void noteInk(float baselineY, float size) {
        noteSpan(baselineY - 0.60f * size, baselineY + 0.00f * size);
    }
    void noteSpan(float top, float bot) {
        if (top < minY) minY = top;
        if (bot > maxY) maxY = bot;
    }
};

void checkExpression(const char* src) {
    RecordingCanvas canvas;
    CachedExpression expr;
    bool parsed = expr.setSource(src);
    assert(parsed);

    const float size = 40.0f;
    const float maxWidth = 1000.0f;
    const float yAxis = 500.0f;
    mathlayout::AstFit fit = expr.fit(canvas, size, maxWidth);
    assert(fit.size > 0.0f && fit.width > 0.0f);

    canvas.reset();
    expr.draw(canvas, /*rightX=*/fit.width, yAxis);
    assert(canvas.minY <= canvas.maxY);  // something was drawn

    // All recorded ink must sit inside the reported fit box, with a small
    // tolerance for float noise (well under one pixel at size 40).
    const float eps = 0.5f;
    float top = yAxis - fit.above;
    float bot = yAxis + fit.below;
    if (canvas.minY < top - eps || canvas.maxY > bot + eps) {
        std::printf(
            "fit_extents_test FAILED for \"%s\": ink [%f, %f] vs fit box [%f, %f]\n",
            src, canvas.minY, canvas.maxY, top, bot);
        assert(false);
    }
}

}  // namespace

int main() {
    // The Math page's own four examples: fraction with a radical denominator,
    // scripts, radical over a fraction, and a fraction whose numerator holds
    // binary operators (the center-dot case).
    checkExpression("1/(2+sqrt(3))");
    checkExpression("x^2+2*x+1");
    checkExpression("sqrt(2)/2");
    checkExpression("(1+2)*(3-4)/5");
    // Extra stress: nested fraction and a script on a fraction.
    checkExpression("(1/2)/(3/4)");
    checkExpression("(1/2)^2+sqrt(1/2)");

    std::puts("fit_extents_test: OK");
    return 0;
}
