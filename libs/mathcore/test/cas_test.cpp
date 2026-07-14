// libs/mathcore/test/cas_test.cpp
#include <mathcore/cas/symbolic_engine.h>
#ifdef MATHCORE_ENABLE_EIGENMATH
#include <mathcore/cas/eigenmath_engine.h>
#endif

#include <atomic>
#include <cassert>
#include <cstdio>

static mathcore::cas::Reply run(mathcore::cas::Op op, const std::string& expr, const std::string& var = "x") {
    mathcore::cas::SymbolicEngine eng;
    mathcore::cas::Request req;
    req.op   = op;
    req.expr = expr;
    req.var  = var;
    std::atomic<bool> cancel{false};
    return eng.evaluate(req, cancel);
}

int main() {
    // name() identifies the homegrown backend.
    {
        mathcore::cas::SymbolicEngine eng;
        assert(std::string(eng.name()) == "symbolic(dyno)");
    }

    // ── Op::Simplify: constant folding ──────────────────────────────────────
    {
        auto r = run(mathcore::cas::Op::Simplify, "1+1");
        assert(r.ok);
        assert(r.text == "2");
    }
    {
        auto r = run(mathcore::cas::Op::Simplify, "2*3+4");  // (2*3)+4 = 10, fully constant
        assert(r.ok);
        assert(r.text == "10");
    }

    // ── Op::Simplify: 0/1 identity elimination ──────────────────────────────
    {
        auto r = run(mathcore::cas::Op::Simplify, "x*1");
        assert(r.ok);
        assert(r.text == "x");
    }
    {
        auto r = run(mathcore::cas::Op::Simplify, "x+0");
        assert(r.ok);
        assert(r.text == "x");
    }
    {
        auto r = run(mathcore::cas::Op::Simplify, "x*0");
        assert(r.ok);
        assert(r.text == "0");
    }

    // ── Op::Derivative: power rule ──────────────────────────────────────────
    // d/dx(x^2) = 2*x^(2-1)*1, simplifies to 2*x.
    {
        auto r = run(mathcore::cas::Op::Derivative, "x^2");
        assert(r.ok);
        assert(r.text == "2*x");
    }

    // ── Op::Derivative: chain rule through sqrt ─────────────────────────────
    // d/dx(sqrt(x)) = 1/(2*sqrt(x)) * 1, simplifies to 1/(2*sqrt(x)).
    {
        auto r = run(mathcore::cas::Op::Derivative, "sqrt(x)");
        assert(r.ok);
        assert(r.text == "1/(2*sqrt(x))");
    }

    // ── Op::Derivative: chain rule through sin composed with a power ───────
    // d/dx(sin(x^2)) = cos(x^2) * (2*x^(2-1)*1), simplifies to cos(x^2)*2*x.
    {
        auto r = run(mathcore::cas::Op::Derivative, "sin(x^2)");
        assert(r.ok);
        assert(r.text == "cos(x^2)*2*x");
    }

    // ── Op::Numeric: delegates to mathx::NumericEvaluator ───────────────────
    {
        auto r = run(mathcore::cas::Op::Numeric, "2+2");
        assert(r.ok);
        assert(r.text == "4");
    }

    // ── Op::Integral: honest stub, never crashes or silently no-ops ─────────
    {
        auto r = run(mathcore::cas::Op::Integral, "x^2");
        assert(!r.ok);
        assert(r.error == "\xE2\x88\xAB needs the CAS engine (Giac)");  // "∫ needs the CAS engine (Giac)"
    }

    // ── Syntax error path still reports cleanly ─────────────────────────────
    {
        auto r = run(mathcore::cas::Op::Simplify, "1+");
        assert(!r.ok);
        assert(r.error == "Syntax Error");
    }

#ifdef MATHCORE_ENABLE_EIGENMATH
    // ── Real Eigenmath backend (opt-in: MATHCORE_ENABLE_EIGENMATH) ──────────
    // Smoke-test through the actual vendored engine (not the dyno): name(),
    // a simple derivative, and a simplify(), checking Reply.ok and sane text.
    {
        auto eng = mathcore::cas::makeEigenmathEngine();
        assert(std::string(eng->name()) == "eigenmath");

        mathcore::cas::Request req;
        req.op   = mathcore::cas::Op::Derivative;
        req.expr = "x^2";
        req.var  = "x";
        std::atomic<bool> cancel{false};
        mathcore::cas::Reply rep = eng->evaluate(req, cancel);
        assert(rep.ok);
        assert(!rep.text.empty());
        printf("cas_test: eigenmath d/dx(x^2) -> %s\n", rep.text.c_str());
    }
    {
        auto eng = mathcore::cas::makeEigenmathEngine();
        mathcore::cas::Request req;
        req.op   = mathcore::cas::Op::Simplify;
        req.expr = "1+1";
        std::atomic<bool> cancel{false};
        mathcore::cas::Reply rep = eng->evaluate(req, cancel);
        assert(rep.ok);
        assert(rep.text == "2");
    }
#endif

    printf("cas_test: OK\n");
    return 0;
}
