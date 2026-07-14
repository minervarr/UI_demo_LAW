// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\app\src\main\cpp\mathlayout.cc (same author, AGPLv3).
// Originally adapted for windows_ui_demo's grayscale-only rendering model
// (src/math/math_layout.cpp): a single `float gray` in place of the original
// Color/RGB ink, and windows_ui_demo's own `MathCanvas` adapter in place of
// the original `Canvas`. This mathcore port goes one step further: every
// `MathCanvas&`/`MsdfFont*` reference is replaced by the library-owned
// `mathcore::IMathCanvas&`/`mathcore::IMathFontMetrics*` seam (see
// imath_canvas.h/imath_font_metrics.h), and the single `float gray` becomes a
// full `mathcore::Ink` (see ink.h). Only the AST-to-mathbox builder is
// ported — see math_layout.h's header comment for why the editor-tree
// builder and the UI-chrome helpers were left out. The original's
// `AstN::Num`/`AstN::Neg` cases tinted negative numbers red for the live
// results tape; that colour feature is dropped along with the rest of the
// chroma system, so `buildAst` below produces plain (untinted) leaves and
// `drawAstAt` takes one uniform `Ink` instead of an optional `const Color*`.
//
// mathlayout.cc — builder translating the CAS result AST into the shared
// mathbox IR (mathbox.h), plus the fit/draw entry points.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
//
// ALL typography (TeX spacing/styles, MATH-table fractions/radicals/scripts)
// lives in mathbox; this file only decides WHAT to lay out: the AST builder
// derives delimiters from precedence and detects ^(1/2) radicals.
#include <mathcore/math_layout.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include <mathcore/mathbox.h>
#include <mathcore/ast.h>

namespace mathcore {
namespace mathlayout {
namespace {

namespace mb = mathcore::mathbox;
using mb::MathClass;

// IR leaf for a math symbol by codepoint with a UTF-8 text fallback.
mb::NodePtr symNode(IMathCanvas& c, uint32_t cp, const char* utf8, MathClass cl) {
    const IMathFontMetrics* mf = c.mathFontMetrics();
    uint32_t k = (mf && mf->hasMath()) ? mf->mathKey(cp) : 0;
    return k ? mb::mkGlyph(k, cl) : mb::mkText(utf8, cl);
}

// ── CAS result AST → IR ──────────────────────────────────────────────────────

using AstN = mathx::Node;

std::string numText(double v) {
    if (v == 0.0) return "0";
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.10g", v);
    return buf;
}

int prec(const AstN& n) {
    switch (n.kind) {
        case AstN::Add: case AstN::Sub:                 return 1;
        case AstN::Mul: case AstN::Div: case AstN::Neg: return 2;
        case AstN::Pow:                                 return 3;
        default:                                        return 4;
    }
}
// A radical is written as ^(1/2); detect that to render a radical.
bool isHalf(const AstN& n) {
    if (n.kind == AstN::Num) return n.num == 0.5;
    return n.kind == AstN::Div &&
           n.kids[0]->kind == AstN::Num && n.kids[0]->num == 1.0 &&
           n.kids[1]->kind == AstN::Num && n.kids[1]->num == 2.0;
}

// Inside-out delimiter levels: `delimDepth(n)` = the deepest delimiter nesting
// WITHIN n's rendering, so the pair a node draws sits at level = depth of what
// it wraps and the innermost pair is level 0 (see mathbox delimiter cycling).
int delimDepth(const AstN& n);
int wrapDepth(const AstN& n, int ctx) {
    return (prec(n) < ctx ? 1 : 0) + delimDepth(n);
}
int delimDepth(const AstN& n) {
    switch (n.kind) {
        case AstN::Num: case AstN::Const: case AstN::Var: return 0;
        case AstN::Neg: return wrapDepth(*n.kids[0], 2);
        case AstN::Add: case AstN::Sub:
            return std::max(wrapDepth(*n.kids[0], 1), wrapDepth(*n.kids[1], 2));
        case AstN::Mul:
            return std::max(wrapDepth(*n.kids[0], 2), wrapDepth(*n.kids[1], 2));
        case AstN::Div:   // fraction bar — no parens
            return std::max(delimDepth(*n.kids[0]), delimDepth(*n.kids[1]));
        case AstN::Pow:
            if (isHalf(*n.kids[1])) return delimDepth(*n.kids[0]);   // radical — no parens
            return std::max(wrapDepth(*n.kids[0], 4), delimDepth(*n.kids[1]));
        case AstN::Call: return 1 + delimDepth(*n.kids[0]);          // always wraps the arg
    }
    return 0;
}

mb::NodePtr buildAstImpl(IMathCanvas& c, const AstN& n);

// Wrap in cycled delimiters when precedence demands it (a wrapped subformula
// is TeX class Inner — that's what gives \left(…\right) its breathing room).
mb::NodePtr wrapAst(IMathCanvas& c, const AstN& n, int ctx) {
    auto inner = buildAstImpl(c, n);
    if (prec(n) < ctx) {
        auto d = mb::mk(mb::Node::Delim);
        d->level = delimDepth(n);
        d->cls = MathClass::Inner;
        d->kids.push_back(std::move(inner));
        return d;
    }
    return inner;
}

mb::NodePtr buildAstImpl(IMathCanvas& c, const AstN& n) {
    const IMathFontMetrics* mf = c.mathFontMetrics();
    switch (n.kind) {
        case AstN::Num:
            return mb::mkText(numText(n.num));
        case AstN::Const: {
            uint32_t k = 0;
            if (mf && mf->hasMath()) {
                if (n.name == "pi")     k = mf->mathKey(0x03C0);              // upright π
                else if (n.name == "e") k = mf->mathKey(mb::mathItalicCp('e'));
            }
            if (k) return mb::mkGlyph(k);
            return mb::mkText(n.name == "pi" ? "\xCF\x80" : n.name);
        }
        case AstN::Var: {
            uint32_t k = 0;
            if (mf && mf->hasMath() && n.name.size() == 1) {
                uint32_t cp = mb::mathItalicCp(n.name[0]);
                if (cp) k = mf->mathKey(cp);
            }
            if (k) return mb::mkGlyph(k);
            return mb::mkText(n.name);
        }
        case AstN::Neg: {                       // Bin at list start demotes → tight sign
            auto hb = mb::mk(mb::Node::HBox);
            auto m = symNode(c, 0x2212, "\xE2\x88\x92", MathClass::Bin);
            if (n.kids[0]->kind == AstN::Num) { // −3 → a negative number, tight
                auto t = mb::mkText(numText(n.kids[0]->num));
                hb->kids.push_back(std::move(m));
                hb->kids.push_back(std::move(t));
            } else {
                hb->kids.push_back(std::move(m));
                hb->kids.push_back(wrapAst(c, *n.kids[0], 2));
            }
            return hb;
        }
        case AstN::Add: case AstN::Sub: {
            auto hb = mb::mk(mb::Node::HBox);
            hb->kids.push_back(wrapAst(c, *n.kids[0], 1));
            hb->kids.push_back(n.kind == AstN::Add
                                   ? symNode(c, 0x002B, "+", MathClass::Bin)
                                   : symNode(c, 0x2212, "\xE2\x88\x92", MathClass::Bin));
            hb->kids.push_back(wrapAst(c, *n.kids[1], 2));
            return hb;
        }
        case AstN::Mul: {   // explicit · — consistent with the input, and it
                            // disambiguates "3·x^n" from "(3·x)^n".
            auto hb = mb::mk(mb::Node::HBox);
            hb->kids.push_back(wrapAst(c, *n.kids[0], 2));
            hb->kids.push_back(symNode(c, 0x22C5, "\xE2\x8B\x85", MathClass::Bin));
            hb->kids.push_back(wrapAst(c, *n.kids[1], 2));
            return hb;
        }
        case AstN::Div: {
            auto f = mb::mk(mb::Node::Frac);
            f->cls = MathClass::Inner;
            f->kids.push_back(buildAstImpl(c, *n.kids[0]));
            f->kids.push_back(buildAstImpl(c, *n.kids[1]));
            return f;
        }
        case AstN::Pow: {
            if (isHalf(*n.kids[1])) {
                auto r = mb::mk(mb::Node::Radical);
                r->kids.push_back(buildAstImpl(c, *n.kids[0]));
                return r;
            }
            auto sc = mb::mk(mb::Node::Script);
            auto base = wrapAst(c, *n.kids[0], 4);
            sc->cls = base->cls;
            sc->kids.push_back(std::move(base));
            sc->kids.push_back(buildAstImpl(c, *n.kids[1]));
            return sc;
        }
        case AstN::Call: {
            if (n.name == "sqrt") {
                auto r = mb::mk(mb::Node::Radical);
                r->kids.push_back(buildAstImpl(c, *n.kids[0]));
                return r;
            }
            auto hb = mb::mk(mb::Node::HBox);
            hb->kids.push_back(mb::mkText(n.name, MathClass::Op));
            auto d = mb::mk(mb::Node::Delim);
            d->level = delimDepth(*n.kids[0]);
            d->cls = MathClass::Inner;
            d->kids.push_back(buildAstImpl(c, *n.kids[0]));
            hb->kids.push_back(std::move(d));
            return hb;
        }
    }
    return mb::mkText("?");
}

}  // namespace

// ── Entry points ─────────────────────────────────────────────────────────────

mb::NodePtr buildAst(IMathCanvas& c, const mathx::Node& root) {
    return buildAstImpl(c, root);
}

AstFit fitAst(IMathCanvas& c, const mathx::Node& root, float maxSize, float maxWidth) {
    mb::NodePtr t = buildAstImpl(c, root);
    float size = maxSize;
    mb::Box b = mb::layout(c, *t, size);
    if (b.w > maxWidth && b.w > 0.0f) size = std::min(size, size * maxWidth / b.w);
    // Only a GENTLE shrink (<=15%) to swallow near-misses; anything wider stays
    // at this size and OVERFLOWS so the caller can scroll it. A low floor
    // crushed long equations to fit exactly (width==maxW → overflow 0).
    size = std::max(size, maxSize * 0.85f);
    b = mb::layout(c, *t, size);
    return {size, b.w, b.above, b.below};
}

void drawAstAt(IMathCanvas& c, const mathx::Node& root, float rightX, float yAxis, float size,
              Ink ink) {
    mb::NodePtr t = buildAstImpl(c, root);
    mb::Box b = mb::layout(c, *t, size);
    mb::draw(c, *t, rightX - b.w, yAxis, ink);   // right-aligned at rightX
}

}  // namespace mathlayout
}  // namespace mathcore
