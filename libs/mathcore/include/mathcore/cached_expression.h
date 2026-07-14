// New file (no direct calculator-app ancestor) — mathcore's own addition,
// motivated by a real performance gap observed while porting this library
// into windows_ui_demo: the Math page's `page_math.cpp` was re-tokenizing,
// re-parsing, AND re-laying-out its 4 hardcoded example strings from scratch
// on every single frame, even though the source strings never change.
// `CachedExpression` is the fix, living inside the library (per the plan's
// "Performance: CachedExpression, living inside the library" design) so every
// consumer (this app's static examples, a future grapher's live-typed
// formula, a future calculator's result tape) gets it for free instead of
// reinventing it per host.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
#pragma once
#include <string>

#include <mathcore/ast.h>
#include <mathcore/ink.h>
#include <mathcore/imath_canvas.h>
#include <mathcore/mathbox.h>
#include <mathcore/math_layout.h>

namespace mathcore {

// Opt-in caching wrapper around tokenize()/parse()/mathlayout::fitAst() /
// mathlayout::buildAst(). Raw tokenize/parse/fitAst/drawAstAt remain directly
// callable for callers that don't want caching (e.g. a one-shot CLI tool);
// this class is for anyone re-typesetting the same source across frames.
//
// - setSource() only re-tokenizes/re-parses when the source string actually
//   changed since the last call (byte-for-byte string compare). It is safe
//   (and intended) to call every frame with the same literal string — that
//   keeps the caller side simple (no caller-owned "did I already call this"
//   guard needed).
// - fit() only re-tokenizes/re-parses if setSource() hasn't already produced
//   a current AST for the current source (defensive — normally setSource()
//   already did this), and only rebuilds+re-lays-out the mathbox IR tree if
//   the source, maxSize, or maxWidth differ from the last fit() call.
// - draw() never re-parses or re-lays-out; it only walks the already-laid-out
//   cached mathbox tree via mathbox::draw() — cheap every frame.
class CachedExpression {
public:
    // Set the expression source. Returns false (and leaves all prior cached
    // state — AST, laid-out tree, last fit result — completely untouched) if
    // the new source fails to tokenize or parse. Returns true (a no-op
    // besides the string compare) if `expr` is byte-identical to the current
    // source. Safe to call every frame with the same literal.
    bool setSource(std::string expr);

    // Re-tokenizes/re-parses only if the source changed since the last call
    // that actually parsed (i.e. only does real work once per distinct
    // source — see setSource()'s own guard, which normally already handles
    // this). Re-runs mathlayout::buildAst() + mathbox::layout() (the
    // "re-lays-out" step) only if source, maxSize, or maxWidth differ from
    // the previous fit() call. Returns the fitted size/extents so the caller
    // can reserve space before drawing.
    mathlayout::AstFit fit(IMathCanvas& c, float maxSize, float maxWidth);

    // Draw the cached, already-laid-out tree. Always cheap (no parse, no
    // layout) — safe to call every frame. fit() must have been called first
    // (with the same canvas) for the tree to exist; a call before any
    // successful setSource()/fit() is a silent no-op.
    void draw(IMathCanvas& c, float rightX, float yAxis, Ink ink = Ink::gray(0.9f)) const;

    // The currently cached parsed AST root (nullptr if no successful
    // setSource() has happened yet). Real, first-class accessor -- lets a
    // caller that also wants to e.g. evaluate the same expression reuse this
    // already-parsed tree instead of re-tokenizing/re-parsing a second time
    // (see src/app/page_math.cpp). Also doubles as the test hook
    // cached_expression_test.cpp uses to assert that an unchanged source
    // does NOT trigger a re-parse (pointer identity preserved across two
    // fit() calls) and that a changed source DOES (pointer changes).
    const mathx::Node* ast() const { return ast_.get(); }

    // Whether a source string has been successfully set at all.
    bool hasSource() const { return ast_ != nullptr; }

private:
    std::string source_;
    mathx::NodePtr ast_;
    mathbox::NodePtr laidOut_;
    float lastSize_ = -1.0f;
    float lastMaxWidth_ = -1.0f;
    mathlayout::AstFit lastFit_{0, 0, 0, 0};
    bool haveFit_ = false;
};

}  // namespace mathcore
