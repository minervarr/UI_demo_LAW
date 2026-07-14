// Ported (pattern, not verbatim) from C:\Users\incxiuefb\Documents\Files\clone\calculator\core\calc\calc.hh (same author, AGPLv3).
// calc.h — calculator facade bridging UI key presses and the math core.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
//
// Owns the editable expression (a structured 2D editor), the last answer (Ans),
// the result/error state, and the degrees/radians flag. It is UI- and Vulkan-free:
// the app feeds it canonical tokens and renders editor() (while typing) or
// displayText() (after '='). The editor linearizes to the canonical string the
// existing lexer/parser/evaluator already accept.
//
// Unlike the original `Calc`, which hardcoded its own `NumericEvaluator` member,
// this port takes a `const mathx::IEvaluator&` by dependency injection (defaulting
// to an internally-owned `mathx::NumericEvaluator` when none is supplied), so a
// future host can swap in a different evaluator (e.g. a CAS-based one) without
// touching this class.
#pragma once
#include <string>

#include <mathcore/editor.h>
#include <mathcore/evaluator.h>
#include <mathcore/numeric_evaluator.h>

namespace mathcore {
namespace calc {

class Calc {
public:
    // Uses an internally-owned mathx::NumericEvaluator by default.
    Calc();
    // Dependency-injected evaluator (e.g. a future CAS-backed IEvaluator).
    // `eval` must outlive this Calc.
    explicit Calc(const mathx::IEvaluator& eval);

    // token is a canonical action:
    //   "0".."9", ".", "+", "-", "*", "/", "^", "(", ")",
    //   "sin","cos","tan","ln","log",   "pi","e",
    //   "frac","sqrt","nthroot","pow",                 // 2D templates
    //   "left","right","up","down",                    // cursor navigation
    //   "C", "back", "=", "degrad"
    void input(const std::string& token);

    // Pretty UTF-8 string to render when showingResult() (the result/error).
    std::string displayText() const;

    // While editing, the app draws this 2D tree directly (see mathlayout).
    const calcedit::Editor& editor() const { return edit_; }
    bool showingResult() const { return showingResult_; }

    bool   degrees() const { return degrees_; }
    double ans()     const { return ans_; }

    // Non-mutating: evaluate the current expression for a live preview. Returns
    // the formatted result, or "" when empty / incomplete / errored / already
    // showing a result.
    std::string preview() const;

    // Insert the last answer's value (its digits) at the cursor — the "Ans" key.
    void insertAnsValue();

private:
    const mathx::IEvaluator&    evaluator_;
    mathx::NumericEvaluator     ownedEvaluator_;  // used only when no evaluator injected

    calcedit::Editor edit_;        // structured expression being edited
    std::string      result_;      // formatted result or error message
    double           ans_ = 0.0;
    bool             showingResult_ = false;
    bool             degrees_ = false;

    void appendToken(const std::string& token);
    void evaluate();
    void clear();
    void backspace();
};

}  // namespace calc
}  // namespace mathcore
