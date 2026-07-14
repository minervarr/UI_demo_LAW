// Unit test for CachedExpression's caching contract, against the fake
// IMathCanvas/IMathFontMetrics backend (fake_math_backend.h, reused from
// Task 3) — no real font/rendering, just proves the caching logic itself.
//
// Verification strategy chosen: pointer identity of the cached AST root
// (CachedExpression::ast()), rather than an injected call counter.
// Rationale: the AST is produced by mathx::parse(), which always heap-
// allocates a fresh mathx::Node tree — so "did setSource()/fit() actually
// re-parse" is exactly equivalent to "did the returned mathx::Node* change",
// with no risk of a counter under/over-counting relative to what the class
// really does internally. It's also directly inspectable without adding any
// instrumentation-only code path to the production class.
#include <cassert>
#include <cstdio>
#include <string>

#include <mathcore/ast.h>
#include <mathcore/cached_expression.h>

#include "fake_math_backend.h"

using namespace mathcore;

int main() {
    test::FakeMathCanvas canvas(/*withFont=*/true);

    CachedExpression expr;

    // 1) setSource() with a syntactically invalid expression fails, and
    // leaves the (empty, not-yet-set) prior state untouched. Note a plain
    // unmatched '(' is NOT a good test case here: parser.h documents that a
    // missing ')' at end is tolerated (auto-closed) -- "+*/" is genuinely
    // unparseable (operator with no left/right operand), so use that.
    bool badOk = expr.setSource("+*/");
    assert(!badOk);
    assert(!expr.hasSource());
    assert(expr.ast() == nullptr);

    // 2) First successful setSource() + fit() actually parses.
    assert(expr.setSource("1/(2+sqrt(3))"));
    assert(expr.hasSource());
    const mathx::Node* astAfterFirstSet = expr.ast();
    assert(astAfterFirstSet != nullptr);

    mathlayout::AstFit fit1 = expr.fit(canvas, 48.0f, 400.0f);
    assert(fit1.size > 0.0f);
    assert(fit1.width > 0.0f);
    // fit() must not have re-parsed (setSource() already did) -- same AST
    // pointer.
    assert(expr.ast() == astAfterFirstSet);

    // 3) Calling fit() again with an UNCHANGED source/size/maxWidth must not
    // re-parse: AST pointer identity preserved.
    mathlayout::AstFit fit2 = expr.fit(canvas, 48.0f, 400.0f);
    assert(expr.ast() == astAfterFirstSet);
    assert(fit2.size == fit1.size);
    assert(fit2.width == fit1.width);
    assert(fit2.above == fit1.above);
    assert(fit2.below == fit1.below);

    // 3b) Re-calling setSource() with the exact same literal string every
    // "frame" (the intended page_math.cpp usage pattern) must also not
    // re-parse.
    assert(expr.setSource("1/(2+sqrt(3))"));
    assert(expr.ast() == astAfterFirstSet);

    // 3c) draw() must be callable with no crash and no observable state
    // change (cheap walk of the cached tree).
    expr.draw(canvas, /*rightX=*/400.0f, /*yAxis=*/100.0f);
    assert(expr.ast() == astAfterFirstSet);

    // 4) Changing the source string DOES trigger a re-parse: pointer
    // changes.
    assert(expr.setSource("sqrt(2)/2"));
    const mathx::Node* astAfterSecondSet = expr.ast();
    assert(astAfterSecondSet != nullptr);
    assert(astAfterSecondSet != astAfterFirstSet);

    mathlayout::AstFit fit3 = expr.fit(canvas, 48.0f, 400.0f);
    assert(fit3.size > 0.0f);
    assert(expr.ast() == astAfterSecondSet);
    expr.draw(canvas, 400.0f, 100.0f);

    // 5) A failed setSource() after a successful one leaves the prior
    // (successful) state completely untouched.
    const mathx::Node* astBeforeBadSet = expr.ast();
    // (source_ itself isn't exposed; re-supplying the same known-good string
    // below and confirming it's treated as a no-op re-affirms nothing was
    // clobbered by the failed setSource() call.)
    assert(!expr.setSource("+*/"));
    assert(expr.ast() == astBeforeBadSet);
    assert(expr.setSource("sqrt(2)/2"));  // re-supplying the still-current
                                           // source is a no-op, confirming
                                           // source_ was never overwritten
                                           // by the failed call above.
    assert(expr.ast() == astBeforeBadSet);

    // 6) Changing maxSize or maxWidth (source unchanged) DOES trigger a
    // re-layout (different AstFit), even though the AST pointer itself
    // stays the same (no re-parse needed, only re-layout).
    mathlayout::AstFit fit4 = expr.fit(canvas, 96.0f, 400.0f);  // bigger maxSize
    assert(expr.ast() == astAfterSecondSet);  // still no re-parse
    assert(fit4.size > fit3.size);                   // but did re-layout at
                                                       // the new size

    std::puts("cached_expression_test: OK");
    return 0;
}
