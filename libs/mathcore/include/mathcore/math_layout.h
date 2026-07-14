// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\app\src\main\cpp\mathlayout.hh (same author, AGPLv3).
// Originally adapted for windows_ui_demo's grayscale-only rendering model
// (src/math/math_layout.h): a single `float gray` (default 0.9f, matching
// mathbox.h's `Node::gray`) in place of the original Color/RGB ink, and
// windows_ui_demo's own `MathCanvas` adapter in place of the original
// `Canvas`. This mathcore port goes one step further: `MathCanvas&` becomes
// the library-owned `mathcore::IMathCanvas&` (see imath_canvas.h), and the
// single `float gray` becomes a full `mathcore::Ink` (see ink.h) — mathcore
// itself is not grayscale-only, only windows_ui_demo's own call sites are
// (via `Ink::gray`). As before, the original file has TWO independent
// builders — one for the live input editor tree (`buildEdRow`/`drawEditor`),
// one for the parsed CAS-result AST (`buildAst`/`fitAst`/`drawAstAt`) — plus
// UI-chrome helpers for equation labels and keypad icons. This app has no
// interactive equation editor, only static showcase examples driven by
// parsing a fixed string, so ONLY the AST-half is ported here; the
// editor-tree half and the UI-chrome helpers (`buildEqLabelIR`, `drawKeyIcon`,
// `drawEqLabel`, `drawVarName`, `drawGhostLhs`) are out of scope and were not
// ported. The original's negative-number red tint and delimiter
// colour-cycling cosmetics are dropped with the rest of the colour system
// (grayscale carries no chroma at windows_ui_demo's call sites; mathcore
// itself carries no chroma opinion here); the delimiter SHAPE cycling (level
// -> ( ) [ ] { }) is structural and kept in mathbox.
//
// mathlayout.hh — 2D ("natural display") rendering entry points.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
//
// Thin builder translates the parsed CAS-result AST (mathx::Node) into the
// shared TeX-style box IR in mathbox.h, which owns ALL the typography — so
// results render with one geometry by construction.
#pragma once
#include <mathcore/ink.h>
#include <mathcore/imath_canvas.h>
#include <mathcore/mathbox.h>

namespace mathx { struct Node; }

namespace mathcore {
namespace mathlayout {

// Fitted bounding box of an AST: the size chosen so width <= maxWidth (<=
// maxSize), and the resulting extents above/below the baseline axis. Lets a
// caller reserve the correct height *before* drawing. Pure measurement — no
// draw, no allocation.
struct AstFit { float size, width, above, below; };
AstFit fitAst(IMathCanvas& c, const mathx::Node& root, float maxSize, float maxWidth);

// Translate a parsed CAS-result AST into the shared mathbox IR (mathbox.h),
// without measuring/laying it out. Exposed (beyond fitAst/drawAstAt, which
// each build+layout their own throwaway tree internally) so a caller that
// wants to keep the built tree around across frames — e.g.
// mathcore::CachedExpression — can build it once, call mathbox::layout() on
// it only when size/maxWidth actually change, and call mathbox::draw() on it
// every frame at near-zero cost.
mathbox::NodePtr buildAst(IMathCanvas& c, const mathx::Node& root);

// Draw an AST at an explicit `size`, right edge at `rightX`, baseline axis at
// `yAxis` (pair with fitAst's size/above to stack rows without overlap).
// `ink` tints the whole expression uniformly (no chroma, so no negative-red
// tinting as in the original).
void drawAstAt(IMathCanvas& c, const mathx::Node& root, float rightX, float yAxis, float size,
              Ink ink = Ink::gray(0.9f));

}  // namespace mathlayout
}  // namespace mathcore
