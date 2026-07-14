// libs/mathcore/test/evaluator_test.cpp
#include <mathcore/numeric_evaluator.h>
#include <mathcore/parser.h>
#include <mathcore/lexer.h>
#include <cassert>
#include <cmath>
#include <cstdio>

static mathx::EvalResult evalStr(const std::string& s, const mathx::EvalContext& ctx = {}) {
    std::vector<mathx::Token> toks;
    bool lexOk = mathx::tokenize(s, toks);
    assert(lexOk);
    mathx::ParseResult pr = mathx::parse(toks);
    assert(pr.ok);
    static mathx::NumericEvaluator eval;
    return eval.eval(*pr.root, ctx);
}

int main() {
    // Basic arithmetic
    {
        auto r = evalStr("2+2");
        assert(r.ok && r.value == 4.0);
    }
    {
        auto r = evalStr("1+2*3");
        assert(r.ok && r.value == 7.0);
    }
    {
        auto r = evalStr("2^3^2");  // right-assoc: 2^(3^2) = 2^9 = 512
        assert(r.ok && r.value == 512.0);
    }
    {
        auto r = evalStr("-5+3");
        assert(r.ok && r.value == -2.0);
    }
    {
        auto r = evalStr("1/(2+3)");
        assert(r.ok && std::fabs(r.value - 0.2) < 1e-12);
    }

    // Constants
    {
        auto r = evalStr("pi");
        assert(r.ok && std::fabs(r.value - 3.14159265358979323846) < 1e-12);
    }
    {
        auto r = evalStr("e");
        assert(r.ok && std::fabs(r.value - 2.71828182845904523536) < 1e-12);
    }

    // Each Call function against a known value (radians default)
    {
        auto r = evalStr("sin(0)");
        assert(r.ok && r.value == 0.0);
    }
    {
        auto r = evalStr("cos(0)");
        assert(r.ok && r.value == 1.0);
    }
    {
        auto r = evalStr("tan(0)");
        assert(r.ok && r.value == 0.0);
    }
    {
        auto r = evalStr("sqrt(9)");
        assert(r.ok && r.value == 3.0);
    }
    {
        auto r = evalStr("ln(e)");
        assert(r.ok && std::fabs(r.value - 1.0) < 1e-12);
    }
    {
        auto r = evalStr("log(100)");
        assert(r.ok && std::fabs(r.value - 2.0) < 1e-12);
    }

    // Division by zero -> ok=false
    {
        auto r = evalStr("1/0");
        assert(!r.ok);
        assert(r.error == "Undefined");
    }

    // sqrt/ln/log out-of-domain -> ok=false ("Math Error")
    {
        auto r = evalStr("sqrt(-1)");
        assert(!r.ok);
        assert(r.error == "Math Error");
    }
    {
        auto r = evalStr("ln(-1)");
        assert(!r.ok);
        assert(r.error == "Math Error");
    }
    {
        auto r = evalStr("ln(0)");
        assert(!r.ok);
        assert(r.error == "Math Error");
    }
    {
        auto r = evalStr("log(-5)");
        assert(!r.ok);
        assert(r.error == "Math Error");
    }
    {
        auto r = evalStr("log(0)");
        assert(!r.ok);
        assert(r.error == "Math Error");
    }

    // Unbound Var -> ok=false
    {
        auto r = evalStr("x+1");
        assert(!r.ok);
        assert(r.error == "Undefined");
    }
    // Bound Var -> ok
    {
        std::vector<std::pair<std::string, double>> vars = {{"x", 5.0}};
        mathx::EvalContext ctx;
        ctx.vars = &vars;
        auto r = evalStr("x+1", ctx);
        assert(r.ok && r.value == 6.0);
    }

    // Degrees vs radians mode changes trig results
    {
        mathx::EvalContext ctxDeg;
        ctxDeg.degrees = true;
        auto r = evalStr("sin(90)", ctxDeg);
        assert(r.ok && std::fabs(r.value - 1.0) < 1e-12);
    }
    {
        mathx::EvalContext ctxRad;  // radians default
        auto r = evalStr("sin(90)", ctxRad);
        assert(r.ok);
        // sin(90 radians) is not 1
        assert(std::fabs(r.value - 1.0) > 1e-6);
    }
    {
        mathx::EvalContext ctxDeg;
        ctxDeg.degrees = true;
        auto r = evalStr("cos(180)", ctxDeg);
        assert(r.ok && std::fabs(r.value - (-1.0)) < 1e-12);
    }

    // Near-zero trig snapping: sin(pi) in radians should read as exactly 0,
    // not representation noise (~1e-16).
    {
        auto r = evalStr("sin(pi)");
        assert(r.ok && r.value == 0.0);
    }
    {
        auto r = evalStr("cos(pi)");
        assert(r.ok && std::fabs(r.value - (-1.0)) < 1e-12);
    }
    // tan(pi) should also snap to 0
    {
        auto r = evalStr("tan(pi)");
        assert(r.ok && r.value == 0.0);
    }

    printf("evaluator_test: OK\n");
    return 0;
}
