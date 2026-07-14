// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\core\graphing\plot.hh (same author, AGPLv3).
// plot.hh — sample an equation into world-space points for the renderer.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
//
// Pure C++17 (uses mathcore/lexer.h, mathcore/parser.h, mathcore/numeric_evaluator.h
// only). The renderer maps these world-space points to screen and draws them, so
// sampling stays decoupled from Vulkan. `valid == false` marks a pen-lift (NaN /
// infinity / undefined / out-of-domain).
#pragma once
#include <string>
#include <vector>

#include <mathcore/graphing/equation.h>

namespace graphing {

struct SamplePt { double wx; double wy; bool valid; };

// Sample `eq` into `out` (cleared first); `count` ≈ the plot's pixel width.
// Function (y=f(x)) walks the world x-range [xmin,xmax]; Parametric (x=X(t),
// y=Y(t)) and Polar (r=R(t)) sweep the parameter t over [eq.tmin,eq.tmax] and
// ignore xmin/xmax. The parameter variable is the ASCII `t`. Returns false only
// if an expression fails to parse.
bool sampleEquation(const Equation& eq, double xmin, double xmax, int count,
                    bool degrees, std::vector<SamplePt>& out);

// Evaluate a Function expression y=f(x) at a single x (for Trace). Returns false
// if it fails to parse or the value is non-finite / undefined.
bool evalFunctionAt(const std::string& expr, double x, bool degrees, double& outY);

}  // namespace graphing
