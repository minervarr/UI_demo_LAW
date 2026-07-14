// libs/mathcore/test/plot_test.cpp
#include <mathcore/graphing/equation.h>
#include <mathcore/graphing/plot.h>
#include <cassert>
#include <cmath>
#include <cstdio>

static constexpr double kPi = 3.14159265358979323846;

int main() {
    // Function: y = x^2 over a small range; each sample should match x*x.
    {
        graphing::Equation eq;
        eq.type = graphing::EqType::Function;
        eq.expr = "x^2";

        std::vector<graphing::SamplePt> out;
        bool ok = graphing::sampleEquation(eq, -3.0, 3.0, 7, /*degrees=*/false, out);
        assert(ok);
        assert(out.size() == 7);
        for (const auto& p : out) {
            assert(p.valid);
            assert(std::fabs(p.wy - p.wx * p.wx) < 1e-9);
        }
    }

    // Function: y = 1/x across a range that includes the x=0 singularity —
    // valid must be false exactly at x=0 and true everywhere else.
    {
        graphing::Equation eq;
        eq.type = graphing::EqType::Function;
        eq.expr = "1/x";

        std::vector<graphing::SamplePt> out;
        bool ok = graphing::sampleEquation(eq, -2.0, 2.0, 5, /*degrees=*/false, out);
        assert(ok);
        assert(out.size() == 5);
        // Grid: x = -2, -1, 0, 1, 2 (index 2 is the singularity).
        for (size_t i = 0; i < out.size(); ++i) {
            if (i == 2) {
                assert(!out[i].valid);
            } else {
                assert(out[i].valid);
                assert(std::fabs(out[i].wy - 1.0 / out[i].wx) < 1e-9);
            }
        }
    }

    // Parametric: x=cos(t), y=sin(t) over [0,2pi] traces a unit circle.
    {
        graphing::Equation eq;
        eq.type  = graphing::EqType::Parametric;
        eq.exprX = "cos(t)";
        eq.exprY = "sin(t)";
        eq.tmin  = 0.0;
        eq.tmax  = 2.0 * kPi;

        std::vector<graphing::SamplePt> out;
        bool ok = graphing::sampleEquation(eq, 0.0, 0.0, 32, /*degrees=*/false, out);
        assert(ok);
        assert(out.size() == 32);
        for (const auto& p : out) {
            assert(p.valid);
            double r2 = p.wx * p.wx + p.wy * p.wy;
            assert(std::fabs(r2 - 1.0) < 1e-9);
        }
    }

    // Polar: r(theta) = 1 over [0,2pi] converts to the same unit circle.
    {
        graphing::Equation eq;
        eq.type = graphing::EqType::Polar;
        eq.expr = "1";
        eq.tmin = 0.0;
        eq.tmax = 2.0 * kPi;

        std::vector<graphing::SamplePt> out;
        bool ok = graphing::sampleEquation(eq, 0.0, 0.0, 32, /*degrees=*/false, out);
        assert(ok);
        assert(out.size() == 32);
        for (const auto& p : out) {
            assert(p.valid);
            double r2 = p.wx * p.wx + p.wy * p.wy;
            assert(std::fabs(r2 - 1.0) < 1e-9);
        }
    }

    // evalFunctionAt: single-point evaluation, parse-once fast path.
    {
        double y = 0.0;
        bool ok = graphing::evalFunctionAt("x^2", 4.0, /*degrees=*/false, y);
        assert(ok);
        assert(std::fabs(y - 16.0) < 1e-9);
    }
    {
        double y = 0.0;
        bool ok = graphing::evalFunctionAt("1/x", 0.0, /*degrees=*/false, y);
        assert(!ok);
    }

    printf("plot_test: OK\n");
    return 0;
}
