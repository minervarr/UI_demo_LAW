// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\app\src\main\cpp\mathlayout.cc's
// `buildEdRow` (~lines 165-269) and `drawEditor` (~lines 435-459), plus the
// helpers it calls: `prettyAtom`/`atomMathCp`/`atomClass`/`atomMathKey`/
// `atomNode` (~lines 30-90, calculator-specific canonical-atom vocabulary),
// `parenLevels` (~lines 113-144), and `isScriptBase` (~lines 146-158).
// Originally adapted for windows_ui_demo's grayscale-only rendering model;
// this mathcore port goes one step further: `Canvas&`/`MsdfFont*` become
// `mathcore::IMathCanvas&`/`mathcore::IMathFontMetrics*` (see
// imath_canvas.h/imath_font_metrics.h), `Style{caret}` becomes mathbox::draw's
// plain `bool showCaret=true` (see mathbox.h/.cpp's Task 3/11 header
// comments), and the editor tree is the Task 10 port
// (mathcore::calcedit::Row/Node/Cursor/Editor) rather than the original
// calc::editor.hh types. `atomNode`'s glyph lookup is unchanged logic, only
// re-typed onto `IMathFontMetrics::mathKey`. The original's negative-number
// red tint and delimiter colour-cycling cosmetics are dropped with the rest
// of the chroma system (matching math_layout.cpp's precedent); the delimiter
// SHAPE cycling (level -> ( ) [ ] { }) and ghost-closer (`ghostClose`)
// behavior for still-open parens are structural and kept exactly, faithfully
// reproducing parenLevels/buildEdRow's paren-matching algorithm.
//
// mathlayout.cc (editor half) — builder translating the live input editor
// tree into the shared mathbox IR (mathbox.h), plus the fit/draw entry point.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
#include <mathcore/editor_layout.h>

#include <algorithm>

namespace mathcore {
namespace mathlayout {
namespace {

namespace mb = mathcore::mathbox;
using mb::MathClass;
using calcedit::Row;
using EdNode = calcedit::Node;

// ── Canonical-atom mapping (calculator-specific vocabulary, ported as-is) ────

// Canonical ASCII atom -> pretty math glyph for display (the editor keeps the
// ASCII in calcedit::Node::text; this only affects rendering).
std::string prettyAtom(const std::string& s) {
    if (s == "*")  return "\xE2\x8B\x85";  // (centered dot -- explicit product)
    if (s == "/")  return "\xC3\xB7";      // division sign
    if (s == "-")  return "\xE2\x88\x92";  // minus sign
    if (s == "pi") return "\xCF\x80";      // pi
    return s;
}

// Codepoint for a canonical atom that should render as one math-face glyph
// (math-italic variables, axis-placed operators, pi), else 0 -- digits,
// multi-letter names (sin, log) and parentheses stay upright text.
uint32_t atomMathCp(const std::string& s) {
    if (s.size() == 1) {
        char ch = s[0];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) return mb::mathItalicCp(ch);
        switch (ch) {
            case '+': return 0x002B;
            case '-': return 0x2212;
            case '*': return 0x22C5;   // centered dot
            case '/': return 0x00F7;   // division sign
            case '=': return 0x003D;
        }
    }
    if (s == "pi") return 0x03C0;      // pi
    return 0;
}
uint32_t atomMathKey(IMathCanvas& c, const std::string& s) {
    const IMathFontMetrics* mf = c.mathFontMetrics();
    if (!mf || !mf->hasMath()) return 0;
    uint32_t cp = atomMathCp(s);
    return cp ? mf->mathKey(cp) : 0;
}

// TeX atom class of a canonical atom (drives the inter-atom glue in mathbox).
MathClass atomClass(const std::string& s) {
    if (s.size() == 1) {
        switch (s[0]) {
            case '+': case '-': case '*': case '/': return MathClass::Bin;
            case '=':                               return MathClass::Rel;
            case '(':                               return MathClass::Open;
            case ')':                               return MathClass::Close;
            case ',':                               return MathClass::Punct;
        }
    }
    return MathClass::Ord;
}

// IR leaf for a canonical atom: math-face glyph when the atlas has one, else
// upright text of the prettified form.
mb::NodePtr atomNode(IMathCanvas& c, const std::string& s) {
    uint32_t k = atomMathKey(c, s);
    if (k) return mb::mkGlyph(k, atomClass(s));
    return mb::mkText(prettyAtom(s), atomClass(s));
}

// ── Editor tree -> IR ─────────────────────────────────────────────────────────

// A row item that opens / closes a delimiter. Opens are the standalone "(" and
// the function tokens "sin(" / "cos(" / ... (they end in "("); close is ")".
bool opensParen(const EdNode& n) {
    return n.kind == EdNode::Atom && !n.text.empty() && n.text.back() == '(';
}
bool closesParen(const EdNode& n) {
    return n.kind == EdNode::Atom && n.text == ")";
}
// Assign every delimiter ATOM in `row` an INSIDE-OUT cycling level (-1 = not a
// delimiter): a pair wrapping content of depth d is itself level d, so the
// innermost pair is level 0. Unmatched opens (still being typed) get levels
// too, pushed onto `unclosed` innermost-first -- those drive the ghost closers.
void parenLevels(const Row& row, std::vector<int>& level, std::vector<int>& unclosed,
                 std::vector<int>& match) {
    int n = static_cast<int>(row.items.size());
    level.assign(n, -1);
    match.assign(n, -1);                     // matched partner index (both ways)
    struct Open { int idx, depth, maxDepth; };
    std::vector<Open> stack;
    int depth = 0;
    for (int i = 0; i < n; ++i) {
        const EdNode& it = *row.items[i];
        if (opensParen(it)) {
            ++depth;
            for (auto& o : stack) o.maxDepth = std::max(o.maxDepth, depth);
            stack.push_back({i, depth, depth});
        } else if (closesParen(it)) {
            if (!stack.empty()) {
                Open o = stack.back(); stack.pop_back();
                int lv = o.maxDepth - o.depth;
                level[o.idx] = lv;
                level[i]     = lv;
                match[o.idx] = i;
                match[i]     = o.idx;
            }
            if (depth > 0) --depth;
        }
    }
    for (int s = static_cast<int>(stack.size()) - 1; s >= 0; --s) {   // innermost first
        int lv = stack[s].maxDepth - stack[s].depth;
        level[stack[s].idx] = lv;
        unclosed.push_back(lv);
    }
}

// Can this IR node serve as the base of a superscript? Mirrors the editor's
// "previous token ends a value" rule (digits/letters/close-paren/templates).
bool isScriptBase(const mb::Node& n) {
    switch (n.kind) {
        case mb::Node::Frac: case mb::Node::Radical: case mb::Node::Script:
        case mb::Node::Delim: case mb::Node::Placeholder:
            return true;
        case mb::Node::Text: case mb::Node::Glyph:
            return n.cls == MathClass::Ord || n.cls == MathClass::Close;
        default:
            return false;
    }
}

}  // namespace

// Build one editor Row. The editor's Power node has NO base child (it attaches
// to the preceding sibling), so the builder resolves it structurally: pop the
// last emitted item and make it the Script's base -- a dashed Placeholder when
// there is nothing to raise (base backspaced away). `unclosedOut`, when given
// (root row only), receives the levels of still-open parens for ghost closers.
mb::NodePtr buildEdRow(IMathCanvas& c, const Row& row, const calcedit::Cursor& cur,
                       std::vector<int>* unclosedOut) {
    auto hb = mb::mk(mb::Node::HBox);
    bool here = (cur.row == &row);
    if (row.items.empty()) {                    // empty slot -> dashed placeholder
        if (here) hb->kids.push_back(mb::mk(mb::Node::Caret));
        hb->kids.push_back(mb::mk(mb::Node::Placeholder));
        return hb;
    }
    std::vector<int> level, unclosed, match;
    parenLevels(row, level, unclosed, match);
    if (unclosedOut) *unclosedOut = unclosed;
    // MATCHED paren pairs become real Delim nodes so they STRETCH around tall
    // content (fractions inside sin(...)) exactly like the results tape. `tgt`
    // is the HBox currently receiving items -- inside an open pair it's the
    // Delim's inner row. Unmatched (still-typing) parens keep the flat
    // cycled-atom form + ghost closers.
    mb::Node* tgt = hb.get();
    std::vector<mb::Node*> restore;
    std::vector<int>       closeAt;
    for (int i = 0; i < static_cast<int>(row.items.size()); ++i) {
        if (here && cur.index == i) tgt->kids.push_back(mb::mk(mb::Node::Caret));
        const EdNode& it = *row.items[i];
        switch (it.kind) {
            case EdNode::Atom:
                if (level[i] >= 0 && opensParen(it)) {
                    // Every open becomes a stretchy Delim. A matched pair pops
                    // back out at its ")"; an UNMATCHED one wraps the rest of
                    // the row and draws its closer dim -- the ghost, now
                    // stretching with the content instead of staying 1em.
                    std::string name = it.text.substr(0, it.text.size() - 1);
                    if (!name.empty())          // sin( -> upright "sin" + stretchy (
                        tgt->kids.push_back(mb::mkText(name, MathClass::Op));
                    auto d = mb::mk(mb::Node::Delim);
                    d->level = level[i];
                    d->cls = MathClass::Inner;
                    d->ghostClose = (match[i] < 0);
                    d->kids.push_back(mb::mk(mb::Node::HBox));
                    mb::Node* inner = d->kids[0].get();
                    tgt->kids.push_back(std::move(d));
                    if (match[i] >= 0) {        // only matched pairs ever pop
                        restore.push_back(tgt);
                        closeAt.push_back(match[i]);
                    }
                    tgt = inner;
                } else if (level[i] >= 0 && closesParen(it) && match[i] >= 0) {
                    tgt = restore.back();       // the matching close: pop back out
                    restore.pop_back();
                    closeAt.pop_back();
                } else {                        // unmatched ")" and plain atoms
                    tgt->kids.push_back(atomNode(c, it.text));
                }
                break;
            case EdNode::Frac: {
                auto f = mb::mk(mb::Node::Frac);
                f->cls = MathClass::Inner;
                f->kids.push_back(buildEdRow(c, it.slots[0], cur));
                f->kids.push_back(buildEdRow(c, it.slots[1], cur));
                tgt->kids.push_back(std::move(f));
                break;
            }
            case EdNode::Sqrt: {
                auto r = mb::mk(mb::Node::Radical);
                r->kids.push_back(buildEdRow(c, it.slots[0], cur));
                tgt->kids.push_back(std::move(r));
                break;
            }
            case EdNode::NthRoot: {
                auto r = mb::mk(mb::Node::Radical);
                r->kids.push_back(buildEdRow(c, it.slots[1], cur));   // radicand
                r->kids.push_back(buildEdRow(c, it.slots[0], cur));   // degree
                tgt->kids.push_back(std::move(r));
                break;
            }
            case EdNode::Power: {
                bool caretBetween = false;      // cursor sat between base and ^
                if (!tgt->kids.empty() && tgt->kids.back()->kind == mb::Node::Caret) {
                    caretBetween = true;
                    tgt->kids.pop_back();
                }
                mb::NodePtr base;
                if (!tgt->kids.empty() && isScriptBase(*tgt->kids.back())) {
                    base = std::move(tgt->kids.back());
                    tgt->kids.pop_back();
                } else {                        // orphan: dashed placeholder base
                    if (caretBetween) {
                        tgt->kids.push_back(mb::mk(mb::Node::Caret));
                        caretBetween = false;
                    }
                    base = mb::mk(mb::Node::Placeholder);
                }
                auto sc = mb::mk(mb::Node::Script);
                sc->cls = base->cls;            // script inherits its nucleus class
                sc->scriptCaret = caretBetween;
                sc->kids.push_back(std::move(base));
                sc->kids.push_back(buildEdRow(c, it.slots[0], cur));
                tgt->kids.push_back(std::move(sc));
                break;
            }
        }
    }
    if (here && cur.index == static_cast<int>(row.items.size()))
        tgt->kids.push_back(mb::mk(mb::Node::Caret));  // may sit inside a ghost delim
    return hb;
}

// ── Entry point ───────────────────────────────────────────────────────────────

void drawEditor(IMathCanvas& c, const calcedit::Editor& ed, const Rect& panel,
                 bool showCaret) {
    // Unclosed parens render as Delim nodes with a DIM ghost closer built by
    // buildEdRow itself -- so the ghost stretches with the content exactly
    // like a typed closer would.
    mb::NodePtr root = buildEdRow(c, ed.root(), ed.cursor());

    const float base   = panel.h * 0.46f;
    const float availW = panel.w - panel.h * 0.30f;
    const float availH = panel.h * 0.92f;

    float size = base;
    mb::Box b = mb::layout(c, *root, size);
    if (b.w > availW && b.w > 0.0f) size = std::min(size, base * availW / b.w);
    float H = b.above + b.below;
    if (H > availH && H > 0.0f) size = std::min(size, size * availH / H);
    size = std::max(size, base * 0.40f);   // floor; taller content clips to panel

    b = mb::layout(c, *root, size);
    float rightX = panel.x + panel.w - panel.h * 0.18f;
    float xStart = rightX - b.w;
    float yAxis  = panel.y + (panel.h - (b.above + b.below)) * 0.5f + b.above;
    mb::draw(c, *root, xStart, yAxis, Ink::gray(0.9f), showCaret);
}

}  // namespace mathlayout
}  // namespace mathcore
