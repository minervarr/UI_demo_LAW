// New file (no direct calculator-app ancestor) — see cached_expression.h for
// the motivation (fixes page_math.cpp re-tokenizing/re-parsing/re-laying-out
// its static examples every frame).
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
#include <mathcore/cached_expression.h>

#include <algorithm>
#include <vector>

#include <mathcore/lexer.h>
#include <mathcore/parser.h>

namespace mathcore {

namespace mb = mathcore::mathbox;

bool CachedExpression::setSource(std::string expr) {
    if (ast_ && expr == source_) {
        // Already-current source (including the very same string re-supplied
        // every frame by a caller like page_math.cpp) — no-op.
        return true;
    }

    std::vector<mathx::Token> toks;
    if (!mathx::tokenize(expr, toks)) return false;
    mathx::ParseResult pr = mathx::parse(toks);
    if (!pr.ok || !pr.root) return false;

    // Only commit on success — a failed parse leaves all prior cached state
    // (source_, ast_, laidOut_, lastFit_) completely untouched, per contract.
    source_ = std::move(expr);
    ast_ = std::move(pr.root);
    // The AST changed, so any previously laid-out tree/fit result is stale;
    // invalidate it so the next fit() call rebuilds+re-lays-out.
    laidOut_.reset();
    haveFit_ = false;
    lastSize_ = -1.0f;
    lastMaxWidth_ = -1.0f;
    return true;
}

mathlayout::AstFit CachedExpression::fit(IMathCanvas& c, float maxSize, float maxWidth) {
    if (!ast_) return {0, 0, 0, 0};

    if (haveFit_ && laidOut_ && lastSize_ == maxSize && lastMaxWidth_ == maxWidth) {
        // Source, size, and maxWidth are all unchanged since the last fit()
        // — cache hit, no rebuild/re-layout.
        return lastFit_;
    }

    // Re-lay-out: build the mathbox IR tree once (buildAst does not consult
    // maxSize/maxWidth — cheap relative to layout()) and cache it, then run
    // the same shrink-to-fit search mathlayout::fitAst() uses internally,
    // but operating on our OWN cached tree via mathbox::layout() directly
    // instead of rebuilding a throwaway tree each call as fitAst() does.
    laidOut_ = mathlayout::buildAst(c, *ast_);

    float size = maxSize;
    mb::Box b = mb::layout(c, *laidOut_, size);
    if (b.w > maxWidth && b.w > 0.0f) size = std::min(size, size * maxWidth / b.w);
    // Only a GENTLE shrink (<=15%) to swallow near-misses; anything wider
    // stays at this size and OVERFLOWS so the caller can scroll it — mirrors
    // mathlayout::fitAst()'s own floor exactly (see math_layout.cpp).
    size = std::max(size, maxSize * 0.85f);
    b = mb::layout(c, *laidOut_, size);

    lastFit_ = {size, b.w, b.above, b.below};
    lastSize_ = maxSize;
    lastMaxWidth_ = maxWidth;
    haveFit_ = true;
    return lastFit_;
}

void CachedExpression::draw(IMathCanvas& c, float rightX, float yAxis, Ink ink) const {
    if (!laidOut_ || !haveFit_) return;
    // Right-align at rightX, same as mathlayout::drawAstAt() — walks the
    // already-laid-out cached tree, no parse/build/layout work here.
    mb::draw(c, *laidOut_, rightX - lastFit_.width, yAxis, ink);
}

}  // namespace mathcore
