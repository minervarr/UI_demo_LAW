// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\core\graphing\plot.cc (same author, AGPLv3).
// plot.cc — equation sampling via the numeric evaluator.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
//
// Ported faithfully from the original (no bug fixes). The original already
// parsed each expression exactly ONCE before the sampling loop, then rebound
// the variable binding (x, t, or θ) per sample and evaluated against the
// already-parsed AST. This port preserves that same structure: tokenize+parse
// once, then per sample rebind EvalContext::vars and eval directly via
// NumericEvaluator::eval. (The original's TODO(perf, M7) comment was
// ambiguous but did not describe an actual per-sample reparse defect.)
#include <mathcore/graphing/plot.h>

#include <cmath>
#include <utility>

#include <mathcore/lexer.h>
#include <mathcore/numeric_evaluator.h>
#include <mathcore/parser.h>

namespace graphing {
namespace {

// y = f(x): evaluate across [xmin,xmax] at `count` evenly spaced columns.
bool sampleFunction(const std::string& expr, double xmin, double xmax, int count,
                    bool degrees, std::vector<SamplePt>& out) {
    std::vector<mathx::Token> toks;
    if (!mathx::tokenize(expr, toks)) return false;
    mathx::ParseResult pr = mathx::parse(toks);
    if (!pr.ok || !pr.root) return false;

    mathx::NumericEvaluator ev;
    std::vector<std::pair<std::string, double>> vars{{"x", 0.0}};
    mathx::EvalContext ctx;
    ctx.degrees = degrees;
    ctx.vars    = &vars;

    if (count < 2) count = 2;
    out.reserve(out.size() + static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        double wx = xmin + (xmax - xmin) * (double)i / (double)(count - 1);
        vars[0].second = wx;
        mathx::EvalResult r = ev.eval(*pr.root, ctx);
        bool valid = r.ok && std::isfinite(r.value);
        out.push_back({wx, valid ? r.value : 0.0, valid});
    }
    return true;
}

// x = X(t), y = Y(t): sweep the parameter t over [tmin,tmax]. Both expressions
// use the ASCII variable `t` (the lexer can't tokenize a multibyte θ).
bool sampleParametric(const std::string& ex, const std::string& ey,
                      double tmin, double tmax, int count, bool degrees,
                      std::vector<SamplePt>& out) {
    std::vector<mathx::Token> tx, ty;
    if (!mathx::tokenize(ex, tx) || !mathx::tokenize(ey, ty)) return false;
    mathx::ParseResult px = mathx::parse(tx), py = mathx::parse(ty);
    if (!px.ok || !px.root || !py.ok || !py.root) return false;

    mathx::NumericEvaluator ev;
    std::vector<std::pair<std::string, double>> vars{{"t", 0.0}};
    mathx::EvalContext ctx;
    ctx.degrees = degrees;
    ctx.vars    = &vars;

    if (count < 2) count = 2;
    out.reserve(out.size() + static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        double t = tmin + (tmax - tmin) * (double)i / (double)(count - 1);
        vars[0].second = t;
        mathx::EvalResult rx = ev.eval(*px.root, ctx);
        mathx::EvalResult ry = ev.eval(*py.root, ctx);
        bool valid = rx.ok && ry.ok && std::isfinite(rx.value) && std::isfinite(ry.value);
        out.push_back({valid ? rx.value : 0.0, valid ? ry.value : 0.0, valid});
    }
    return true;
}

// r = R(t): sweep the angle t over [tmin,tmax], convert to Cartesian. The
// geometric angle honors the same unit as the global mode, so the sweep and any
// trig inside the expression agree (t is radians in RAD, degrees in DEG).
bool samplePolar(const std::string& expr, double tmin, double tmax, int count,
                 bool degrees, std::vector<SamplePt>& out) {
    std::vector<mathx::Token> toks;
    if (!mathx::tokenize(expr, toks)) return false;
    mathx::ParseResult pr = mathx::parse(toks);
    if (!pr.ok || !pr.root) return false;

    mathx::NumericEvaluator ev;
    std::vector<std::pair<std::string, double>> vars{{"t", 0.0}};
    mathx::EvalContext ctx;
    ctx.degrees = degrees;
    ctx.vars    = &vars;

    constexpr double kDeg2Rad = 0.017453292519943295;
    if (count < 2) count = 2;
    out.reserve(out.size() + static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        double th = tmin + (tmax - tmin) * (double)i / (double)(count - 1);
        vars[0].second = th;
        mathx::EvalResult r = ev.eval(*pr.root, ctx);
        bool valid = r.ok && std::isfinite(r.value);
        double ang = degrees ? th * kDeg2Rad : th;
        double x = valid ? r.value * std::cos(ang) : 0.0;
        double y = valid ? r.value * std::sin(ang) : 0.0;
        out.push_back({x, y, valid});
    }
    return true;
}

}  // namespace

bool sampleEquation(const Equation& eq, double xmin, double xmax, int count,
                    bool degrees, std::vector<SamplePt>& out) {
    out.clear();
    switch (eq.type) {
        case EqType::Function:
            return sampleFunction(eq.expr, xmin, xmax, count, degrees, out);
        case EqType::Parametric:
            return sampleParametric(eq.exprX, eq.exprY, eq.tmin, eq.tmax, count, degrees, out);
        case EqType::Polar:
            return samplePolar(eq.expr, eq.tmin, eq.tmax, count, degrees, out);
    }
    return false;
}

bool evalFunctionAt(const std::string& expr, double x, bool degrees, double& outY) {
    std::vector<mathx::Token> toks;
    if (!mathx::tokenize(expr, toks)) return false;
    mathx::ParseResult pr = mathx::parse(toks);
    if (!pr.ok || !pr.root) return false;

    mathx::NumericEvaluator ev;
    std::vector<std::pair<std::string, double>> vars{{"x", x}};
    mathx::EvalContext ctx;
    ctx.degrees = degrees;
    ctx.vars    = &vars;
    mathx::EvalResult r = ev.eval(*pr.root, ctx);
    if (!r.ok || !std::isfinite(r.value)) return false;
    outY = r.value;
    return true;
}

}  // namespace graphing
