// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\core\math\numeric_evaluator.hh (same author, AGPLv3).
// numeric_evaluator.hh — double-precision numeric evaluator.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
#pragma once
#include <mathcore/evaluator.h>

namespace mathx {

class NumericEvaluator : public IEvaluator {
public:
    EvalResult eval(const Node& root, const EvalContext& ctx) const override;
};

}  // namespace mathx
