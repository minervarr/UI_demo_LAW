// libs/mathcore/test/calc_test.cpp
#include <mathcore/calc.h>
#include <mathcore/numeric_evaluator.h>
#include <cassert>
#include <cmath>
#include <cstdio>

using mathcore::calc::Calc;
using namespace mathcore::calcedit;

static void test_tokenDispatchBuildsRow() {
    Calc c;  // default-owned NumericEvaluator
    c.input("2");
    c.input("+");
    c.input("3");
    assert(c.editor().linearize() == "2+3");
    // Row shape: three atoms "2", "+", "3".
    const Row& root = c.editor().root();
    assert(root.items.size() == 3);
    assert(root.items[0]->text == "2");
    assert(root.items[1]->text == "+");
    assert(root.items[2]->text == "3");
}

static void test_evaluateReturnsCorrectResult() {
    mathx::NumericEvaluator ev;
    Calc c(ev);
    c.input("2");
    c.input("+");
    c.input("3");
    c.input("=");
    assert(c.showingResult());
    assert(c.displayText() == "5");
    assert(c.ans() == 5.0);
}

static void test_evaluateFunctionCall() {
    mathx::NumericEvaluator ev;
    Calc c(ev);
    // "sqrt" is a 2D template token: it opens a structural Sqrt node whose
    // linearize() always emits a balanced "sqrt(...)".
    c.input("sqrt");
    c.input("9");
    c.input("=");
    assert(c.showingResult());
    assert(c.displayText() == "3");
}

// Ans-injection: after a trailing operator, '=' should append Ans before
// evaluating. Also cover appendToken's "continue from Ans" UX: pressing an
// operator right after a shown result restarts the row from Ans's digits,
// and the operator itself is still appended afterward.
static void test_ansInjectionOnTrailingOperator() {
    mathx::NumericEvaluator ev;
    Calc c(ev);
    c.input("2");
    c.input("+");
    c.input("3");
    c.input("=");
    assert(c.ans() == 5.0);

    // '=' on "2+3+" (trailing operator) should auto-append Ans. c2 is a fresh
    // Calc whose ans_ is still 0, so this reads "2+3+0" -> 5.
    Calc c2(ev);
    c2.input("2");
    c2.input("+");
    c2.input("3");
    c2.input("+");
    c2.input("=");
    assert(c2.showingResult());
    assert(c2.displayText() == "5");

    // Continue-from-Ans UX: after a shown result, pressing an operator
    // restarts the editor row from Ans's digits, then appends the operator.
    // So typing "+","5","=" after ans=5 should compute 5+5=10.
    assert(c.showingResult());
    c.input("+");
    assert(!c.showingResult());
    assert(c.editor().linearize() == "5+");
    c.input("5");
    assert(c.editor().linearize() == "5+5");
    c.input("=");
    assert(c.showingResult());
    assert(c.displayText() == "10");
    assert(c.ans() == 10.0);

    // Directly exercise evaluate()'s own trailing-operator Ans-append (as
    // opposed to appendToken's continue-from-Ans restart above): with a
    // nonzero ans_, evaluating a row that ends in an operator should append
    // Ans's value before parsing, not the operator's restart logic.
    Calc c3(ev);
    c3.input("2");
    c3.input("+");
    c3.input("3");
    c3.input("=");
    assert(c3.ans() == 5.0);
    c3.input("+");             // continue-from-Ans restart -> row is now "5+"
    assert(c3.editor().linearize() == "5+");
    c3.input("=");              // trailing operator -> evaluate() appends Ans (5) -> "5+5"
    assert(c3.showingResult());
    assert(c3.displayText() == "10");
}

static void test_autoParenBalancing() {
    mathx::NumericEvaluator ev;
    Calc c(ev);
    c.input("(");
    c.input("1");
    c.input("+");
    c.input("2");
    // No closing paren typed.
    assert(c.editor().linearize() == "(1+2");
    c.input("=");
    assert(c.showingResult());
    assert(c.displayText() == "3");  // behaves as if "(1+2)" was evaluated
}

static void test_previewDoesNotMutateState() {
    mathx::NumericEvaluator ev;
    Calc c(ev);
    c.input("2");
    c.input("+");
    c.input("3");
    std::string before = c.editor().linearize();
    bool showingBefore = c.showingResult();

    std::string p = c.preview();
    assert(p == "5");

    // State unchanged after preview().
    assert(c.editor().linearize() == before);
    assert(c.showingResult() == showingBefore);

    // Calling preview() again then evaluate() is consistent.
    std::string p2 = c.preview();
    assert(p2 == p);
    c.input("=");
    assert(c.displayText() == "5");
}

static void test_previewAutoBalancesParens() {
    mathx::NumericEvaluator ev;
    Calc c(ev);
    c.input("(");
    c.input("4");
    // "(4" is unclosed; preview should still balance (to "(4)") and evaluate.
    assert(c.preview() == "4");
    // Still unclosed after preview (non-mutating).
    assert(c.editor().linearize() == "(4");
}

static void test_defaultConstructorOwnsEvaluator() {
    // No evaluator injected -> must still work end-to-end via the owned default.
    Calc c;
    c.input("4");
    c.input("*");
    c.input("2");
    c.input("=");
    assert(c.showingResult());
    assert(c.displayText() == "8");
}

static void test_leadingZeroReplacement() {
    Calc c;
    c.input("0");
    assert(c.editor().linearize() == "0");
    c.input("7");  // replaces the lone leading zero
    assert(c.editor().linearize() == "7");
}

static void test_dotAutoPrefixesZero() {
    Calc c;
    c.input(".");
    assert(c.editor().linearize() == "0.");
}

static void test_clearAndBackspace() {
    Calc c;
    c.input("1");
    c.input("2");
    c.input("back");
    assert(c.editor().linearize() == "1");
    c.input("C");
    assert(c.editor().empty());
}

int main() {
    test_tokenDispatchBuildsRow();
    test_evaluateReturnsCorrectResult();
    test_evaluateFunctionCall();
    test_ansInjectionOnTrailingOperator();
    test_autoParenBalancing();
    test_previewDoesNotMutateState();
    test_previewAutoBalancesParens();
    test_defaultConstructorOwnsEvaluator();
    test_leadingZeroReplacement();
    test_dotAutoPrefixesZero();
    test_clearAndBackspace();
    printf("calc_test: OK\n");
    return 0;
}
