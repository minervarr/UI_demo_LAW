// Compile/link smoke test for math_layout.cpp against the fake IMathCanvas/
// IMathFontMetrics backend (libs/mathcore/test/fake_math_backend.h, the same
// harness Task 3 built for mathbox_link_test.cpp — reused here rather than
// duplicated). Not a functional/visual test; just proves math_layout.cpp
// compiles+links standalone against the mathcore seam with no MsdfFont/
// Vulkan dependency, and exercises buildAst's Num/Const/Var/Neg/Add/Sub/Mul/
// Div/Pow(radical)/Pow(script)/Call cases once each via fitAst/drawAstAt so a
// signature mismatch would fail to compile.
#include <cassert>
#include <cstdio>

#include <mathcore/ast.h>
#include <mathcore/math_layout.h>

#include "fake_math_backend.h"

using namespace mathcore;
using namespace mathcore::mathlayout;
using AstN = mathx::Node;

namespace {

// (x + 1/2) * -3 ^ sqrt(2) / cos(pi)
// Exercises Num/Const/Var/Neg/Add/Sub/Mul/Div/Pow(radical via ^(1/2))/
// Pow(non-radical script)/Call.
mathx::NodePtr buildSample() {
    auto x = std::make_unique<AstN>(AstN::Var);
    x->name = "x";

    auto half = mathx::makeNum(0.5);   // isHalf() detection via a literal 0.5

    auto xPlusHalf = std::make_unique<AstN>(AstN::Add);
    xPlusHalf->kids.push_back(std::move(x));
    xPlusHalf->kids.push_back(std::move(half));

    auto two = mathx::makeNum(2.0);
    auto sqrtTwo = std::make_unique<AstN>(AstN::Call);
    sqrtTwo->name = "sqrt";
    sqrtTwo->kids.push_back(std::move(two));

    auto three = mathx::makeNum(3.0);
    auto negThree = std::make_unique<AstN>(AstN::Neg);
    negThree->kids.push_back(std::move(three));

    auto powNode = std::make_unique<AstN>(AstN::Pow);
    powNode->kids.push_back(std::move(negThree));
    powNode->kids.push_back(std::move(sqrtTwo));   // non-radical exponent (Call, not 1/2)

    auto mulNode = std::make_unique<AstN>(AstN::Mul);
    mulNode->kids.push_back(std::move(xPlusHalf));
    mulNode->kids.push_back(std::move(powNode));

    auto pi = std::make_unique<AstN>(AstN::Const);
    pi->name = "pi";
    auto cosPi = std::make_unique<AstN>(AstN::Call);
    cosPi->name = "cos";
    cosPi->kids.push_back(std::move(pi));

    auto divNode = std::make_unique<AstN>(AstN::Div);
    divNode->kids.push_back(std::move(mulNode));
    divNode->kids.push_back(std::move(cosPi));

    // Also exercise Pow's radical (^(1/2)) branch directly.
    auto y = std::make_unique<AstN>(AstN::Var);
    y->name = "y";
    auto halfExp = mathx::makeNum(0.5);
    auto radicalPow = std::make_unique<AstN>(AstN::Pow);
    radicalPow->kids.push_back(std::move(y));
    radicalPow->kids.push_back(std::move(halfExp));

    auto sub = std::make_unique<AstN>(AstN::Sub);
    sub->kids.push_back(std::move(divNode));
    sub->kids.push_back(std::move(radicalPow));

    return sub;
}

}  // namespace

int main() {
    // With a fake math font present.
    {
        test::FakeMathCanvas canvas(/*withFont=*/true);
        mathx::NodePtr root = buildSample();
        AstFit fit = fitAst(canvas, *root, 48.0f, 400.0f);
        assert(fit.size > 0.0f);
        assert(fit.width > 0.0f);
        drawAstAt(canvas, *root, /*rightX=*/400.0f, /*yAxis=*/100.0f, fit.size);
        drawAstAt(canvas, *root, 400.0f, 100.0f, fit.size, Ink::gray(0.5f));
    }
    // Legacy no-math-font fallback path.
    {
        test::FakeMathCanvas canvas(/*withFont=*/false);
        mathx::NodePtr root = buildSample();
        AstFit fit = fitAst(canvas, *root, 48.0f, 400.0f);
        assert(fit.size > 0.0f);
        drawAstAt(canvas, *root, 400.0f, 100.0f, fit.size);
    }

    std::puts("math_layout_link_test: OK");
    return 0;
}
