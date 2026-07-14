// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\app\src\main\cpp\mathlayout.hh (same author, AGPLv3)
// and mathlayout.cc's `buildEdRow`/`drawEditor` (~lines 165-269, ~435-459).
// Originally adapted for windows_ui_demo's grayscale-only rendering model; this
// mathcore port goes one step further: `Canvas&` becomes the library-owned
// `mathcore::IMathCanvas&` (see imath_canvas.h), the original's `Style{caret}`
// becomes `mathbox::draw`'s plain `bool showCaret` (see mathbox.h's Task 3/11
// header comment), and the editor tree is the Task 10 port
// (mathcore::calcedit::Row/Node/Cursor/Editor, see editor.h) rather than the
// original calc::editor.hh types. `Rect` is new in mathcore (not ported): the
// original took a `const Rect&` from its own canvas.hh; mathcore has no shared
// geometry header, so a minimal `{x,y,w,h}` struct is defined here instead.
// The original's negative-number red tint and delimiter colour-cycling
// cosmetics are dropped with the rest of the chroma system, matching
// math_layout.h's precedent; the delimiter SHAPE cycling (level -> ( ) [ ] { })
// and the ghost-closer (`ghostClose`) behavior for still-open parens are
// structural and kept exactly.
//
// mathlayout.hh (editor half) — bridges a LIVE mathcore::calcedit::Editor
// (Row/Node/Cursor) into the shared mathbox IR (mathbox.h) so the input editor
// renders with the exact same TeX-style geometry as the CAS results tape.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
#pragma once
#include <vector>

#include <mathcore/editor.h>
#include <mathcore/ink.h>
#include <mathcore/imath_canvas.h>
#include <mathcore/mathbox.h>

namespace mathcore {

// New in mathcore (not ported): a minimal axis-aligned rect, standing in for
// the original's own canvas.hh Rect so drawEditor doesn't need to depend on
// any app-side geometry header.
struct Rect { float x = 0, y = 0, w = 0, h = 0; };

namespace mathlayout {

// Translate one live editor Row into the shared mathbox IR as an HBox: walks
// row.items, splicing a Node::Caret at the cursor's exact index (or, for an
// empty row, Caret + Placeholder); recurses into Frac/Sqrt/NthRoot/Power's
// child slots. `unclosedOut`, when given (root row only), receives the
// inside-out cycling levels of still-open (unmatched) parens, in innermost-
// first order — the same list buildEdRow itself consumes internally to mark
// their ghost closers, exposed here only for callers that want it (mirrors
// the original's optional out-param; mathbox itself never reads it).
mathbox::NodePtr buildEdRow(IMathCanvas& c, const calcedit::Row& row,
                            const calcedit::Cursor& cur,
                            std::vector<int>* unclosedOut = nullptr);

// Build+layout+draw a live Editor's current tree into `panel`, right-aligned
// with a small margin, shrinking to fit (floored at 40% of the nominal size)
// exactly like the original. `showCaret` gates mathbox::draw's own showCaret
// flag (default true, matching the original's always-on caret) -- Task 13
// passes a blink toggle here so the live-editor demo's caret actually blinks
// instead of drawing solid every frame.
void drawEditor(IMathCanvas& c, const calcedit::Editor& ed, const Rect& panel,
                 bool showCaret = true);

}  // namespace mathlayout
}  // namespace mathcore
