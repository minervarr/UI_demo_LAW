// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\app\src\main\cpp\mathbox.hh (same author, AGPLv3).
// Originally adapted for windows_ui_demo's grayscale-only rendering model
// (src/math/mathbox.h): a single `float gray` in place of the original
// Canvas's Color, and windows_ui_demo's own `MathCanvas` adapter in place of
// the original `Canvas`. This mathcore port goes one step further: the app-
// specific `MathCanvas` is replaced by the library-owned `mathcore::IMathCanvas`
// interface (see imath_canvas.h/imath_font_metrics.h), and the single `float
// gray` becomes a full `mathcore::Ink` (see ink.h) — mathcore itself is not
// grayscale-only, only windows_ui_demo's own call sites are (via `Ink::gray`).
// The delimiter SHAPE cycling (level -> ( ) [ ] { }) remains structural and is
// kept; the original's colour-cycling / negative-red cosmetics stay dropped.
//
// mathbox.hh — TeX-style math layout core shared by the input editor and the
// CAS results tape.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
//
// A small box IR (HBox/Text/Glyph/Frac/Radical/Script/Delim/Placeholder/Caret)
// laid out with TeX's real rules, every parameter read from the math font's
// OpenType MATH table: the Display/Text/Script/ScriptScript style system, the
// TeXbook ch.18 inter-class spacing table (thin/med/thick muskips), fraction
// shift-up/-down + minimum gaps, and stretchy vertical constructions for the
// radical. Builders translate the CAS AST into this IR; the geometry itself has
// exactly ONE definition here.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <mathcore/ink.h>
#include <mathcore/imath_canvas.h>

namespace mathcore {
namespace mathbox {

// Every element is a box around the shared horizontal math axis (the line
// fraction bars balance on): above/below are the extents up/down from it.
struct Box { float w = 0, above = 0, below = 0; };

// TeX styles. Order matters: >= Script means "script-size context" (glue that
// the spacing table marks suppressible is dropped, like TeX does).
enum class MathStyle : uint8_t { Display, Text, Script, ScriptScript };

// TeXbook ch.18 atom classes driving inter-atom glue.
enum class MathClass : uint8_t { Ord, Op, Bin, Rel, Open, Close, Punct, Inner };

struct Node;
using NodePtr = std::unique_ptr<Node>;

struct Node {
    enum Kind {
        HBox,         // horizontal list; kids spaced by the TeX glue table
        Text,         // upright text via IMathCanvas::text (digits, "sin", pi fallback)
        Glyph,        // one math-face glyph by key (italic vars, axis operators)
        Frac,         // kids = {num, den}
        Radical,      // kids = {radicand[, degree]}
        Script,       // kids = {base, script}; sup by default, sub when `sub`
        Delim,        // kids = {child}; cycled/stretchy open+close pair around it
        Placeholder,  // dashed empty-slot box (editor slots, orphan power base)
        Caret,        // zero-width cursor marker (drawn by the parent HBox/Script)
    };
    Kind        kind;
    std::string text;                  // Text
    uint32_t    key = 0;               // Glyph
    MathClass   cls = MathClass::Ord;  // spacing class when an HBox child
    bool        hasColor = false;      // explicit ink override (ghosts, tints)
    Ink         ink = Ink::gray(0.9f); // leaf ink when hasColor
    int         level = 0;             // Delim: inside-out cycling level (SHAPE)
    bool        scriptCaret = false;   // Script: caret between base and exponent
    bool        sub = false;           // Script: subscript instead of superscript
    bool        ghostClose = false;    // Delim: close drawn dim (unclosed input paren)
    std::vector<NodePtr> kids;

    // Layout cache — filled by layout(), reused by draw().
    Box       box{};
    float     size = 0;                // resolved px size of this node
    float     gapBefore = 0;           // TeX glue, set by the parent HBox
    MathStyle style = MathStyle::Display;

    explicit Node(Kind k) : kind(k) {}
};

inline NodePtr mk(Node::Kind k) { return std::make_unique<Node>(k); }
inline NodePtr mkText(std::string s, MathClass cl = MathClass::Ord) {
    auto n = mk(Node::Text); n->text = std::move(s); n->cls = cl; return n;
}
inline NodePtr mkGlyph(uint32_t key, MathClass cl = MathClass::Ord) {
    auto n = mk(Node::Glyph); n->key = key; n->cls = cl; return n;
}

// Resolve styles/sizes and cache every node's Box. `displaySize` is the px size
// of Display-style material; Script/ScriptScript nodes scale down by the font's
// scriptPercentScaleDown constants. Returns the root box.
Box layout(IMathCanvas& c, Node& n, float displaySize, MathStyle st = MathStyle::Display);

// Draw a laid-out tree with its left edge at x and its math axis at yAxis, at
// the given `ink`. layout() must have run on this tree (same canvas/sizes)
// first. `showCaret` gates drawing of Node::Caret markers (from the parent
// HBox's loop) and Script::scriptCaret bars — root callers default it to
// false; recursive calls thread the same value down.
void draw(IMathCanvas& c, const Node& n, float x, float yAxis, Ink ink, bool showCaret = false);

// ── Shared low-level typography (used by the layout core and the builders) ───
// Math-axis height as a fraction of em (MATH table axisHeight, fallback 0.25).
float axisFrac(IMathCanvas& c);
// Nominal atom ink extents about the math axis.
float atomAbove(IMathCanvas& c, float size);
float atomBelow(IMathCanvas& c, float size);
// Draw text with its BASELINE axisHeight·size below yAxis (math-axis aligned).
void axisText(IMathCanvas& c, std::string_view s, float x, float yAxis, float size, Ink ink);
// Size multiplier for a style (1.0 / scriptPercentScaleDown / scriptScript…).
float styleScale(IMathCanvas& c, MathStyle st);
// Math-italic codepoint for a variable letter (U+1D44E block; italic h = U+210E).
uint32_t mathItalicCp(char ch);
// OpenType-MATH radical: full box of a radical wrapping `radicand`, and drawing
// the surd + vinculum (returns the x where the radicand content starts).
Box   radicalMeasure(IMathCanvas& c, const Box& radicand, float size);
float radicalDraw(IMathCanvas& c, const Box& radicand, float x, float yAxis, float size, Ink ink);
// Dashed placeholder rectangle (editor empty-slot style).
void dashedRect(IMathCanvas& c, float x, float y, float w, float h, Ink ink);
// Nested-delimiter cycling: level counts INSIDE-OUT (innermost pair = 0 = "(").
// Shape cycles ( ) → [ ] → { } every level (structural — kept). The original
// colour cycling every 3 levels is dropped (grayscale carries no chroma at the
// windows_ui_demo call sites; mathcore itself carries no chroma opinion here).
const char* openDelim(int level);
const char* closeDelim(int level);

}  // namespace mathbox
}  // namespace mathcore
