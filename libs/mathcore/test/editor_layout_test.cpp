// Compile/link smoke test for editor_layout.cpp (buildEdRow/drawEditor)
// against the fake IMathCanvas/IMathFontMetrics backend (test/fake_math_backend.h,
// the same harness Task 3/4 built and reused). Not a functional/visual test —
// text rendering is currently broken in this dev environment (unrelated GPU-
// driver issue) so no live keyboard/screenshot verification is possible here;
// these are structural assertions against the built mb::Node tree (kinds,
// counts, caret position) driven through the real Task 10 calcedit::Editor
// (insertAtom/insertFraction/insertPower/backspace/moveLeft), so the cursor
// states below are ones the live editor actually produces, not hand-built.
#include <cassert>
#include <cstdio>

#include <mathcore/editor.h>
#include <mathcore/editor_layout.h>

#include "fake_math_backend.h"

using namespace mathcore;
using namespace mathcore::mathlayout;
using namespace mathcore::calcedit;
namespace mb = mathcore::mathbox;

namespace {

void test_emptyRow() {
    test::FakeMathCanvas canvas;
    Editor ed;
    mb::NodePtr root = buildEdRow(canvas, ed.root(), ed.cursor());
    assert(root->kind == mb::Node::HBox);
    assert(root->kids.size() == 2);
    assert(root->kids[0]->kind == mb::Node::Caret);
    assert(root->kids[1]->kind == mb::Node::Placeholder);
}

void test_flatRowCaretAtEnd() {
    test::FakeMathCanvas canvas;
    Editor ed;
    ed.insertAtom("x");
    ed.insertAtom("+");
    ed.insertAtom("1");
    // cursor sits at the end after three inserts.
    mb::NodePtr root = buildEdRow(canvas, ed.root(), ed.cursor());
    assert(root->kind == mb::Node::HBox);
    assert(root->kids.size() == 4);   // x, +, 1, Caret
    assert(root->kids[3]->kind == mb::Node::Caret);
    for (int i = 0; i < 3; ++i)
        assert(root->kids[i]->kind == mb::Node::Text || root->kids[i]->kind == mb::Node::Glyph);
}

void test_caretMidRow() {
    test::FakeMathCanvas canvas;
    Editor ed;
    ed.insertAtom("x");
    ed.insertAtom("+");
    ed.insertAtom("1");
    ed.moveLeft();
    ed.moveLeft();   // cursor now between x and + (index 1)
    assert(ed.cursor().index == 1);
    mb::NodePtr root = buildEdRow(canvas, ed.root(), ed.cursor());
    assert(root->kids.size() == 4);   // x, Caret, +, 1
    assert(root->kids[0]->kind != mb::Node::Caret);
    assert(root->kids[1]->kind == mb::Node::Caret);
    assert(root->kids[2]->kind != mb::Node::Caret);
    assert(root->kids[3]->kind != mb::Node::Caret);
}

void test_matchedParens() {
    test::FakeMathCanvas canvas;
    Editor ed;
    ed.insertAtom("(");
    ed.insertAtom("x");
    ed.insertAtom(")");
    // cursor at end (index 3).
    mb::NodePtr root = buildEdRow(canvas, ed.root(), ed.cursor());
    assert(root->kids.size() == 2);   // Delim, Caret (matched pair pops back to root)
    assert(root->kids[0]->kind == mb::Node::Delim);
    assert(root->kids[0]->ghostClose == false);
    assert(root->kids[0]->kids.size() == 1);
    mb::Node* inner = root->kids[0]->kids[0].get();
    assert(inner->kind == mb::Node::HBox);
    assert(inner->kids.size() == 1);   // just the x
    assert(root->kids[1]->kind == mb::Node::Caret);
}

void test_unmatchedParenGhostClose() {
    test::FakeMathCanvas canvas;
    Editor ed;
    ed.insertAtom("(");
    ed.insertAtom("x");
    // no closing paren typed — still-open, cursor at end (index 2).
    std::vector<int> unclosed;
    mb::NodePtr root = buildEdRow(canvas, ed.root(), ed.cursor(), &unclosed);
    assert(unclosed.size() == 1);
    assert(unclosed[0] == 0);   // single, innermost open -> level 0
    assert(root->kids.size() == 1);
    mb::Node* d = root->kids[0].get();
    assert(d->kind == mb::Node::Delim);
    assert(d->ghostClose == true);
    // The caret stays "inside" the still-open delim (tgt never restored to root).
    assert(d->kids[0]->kids.size() == 2);   // x, Caret
    assert(d->kids[0]->kids[1]->kind == mb::Node::Caret);
}

void test_fraction() {
    test::FakeMathCanvas canvas;
    Editor ed;
    bool ok = ed.insertFraction();
    assert(ok);
    ed.insertAtom("8");
    ed.moveRight();     // numerator -> denominator
    ed.insertAtom("5");
    mb::NodePtr root = buildEdRow(canvas, ed.root(), ed.cursor());
    assert(root->kids.size() == 1);
    mb::Node* f = root->kids[0].get();
    assert(f->kind == mb::Node::Frac);
    assert(f->kids.size() == 2);
    mb::Node* num = f->kids[0].get();
    mb::Node* den = f->kids[1].get();
    assert(num->kids.size() == 1);          // just "8", no caret (cursor is in den)
    assert(den->kids.size() == 2);          // "5", Caret (cursor at den end)
    assert(den->kids[1]->kind == mb::Node::Caret);
}

void test_nthRootDegreeOrderSwap() {
    test::FakeMathCanvas canvas;
    Editor ed;
    bool ok = ed.insertNthRoot();
    assert(ok);
    ed.insertAtom("3");   // index/degree, typed first
    ed.moveRight();       // -> radicand
    ed.insertAtom("8");
    mb::NodePtr root = buildEdRow(canvas, ed.root(), ed.cursor());
    assert(root->kids.size() == 1);
    mb::Node* r = root->kids[0].get();
    assert(r->kind == mb::Node::Radical);
    assert(r->kids.size() == 2);
    // Brief: radicand pushed FIRST (kids[0]), degree pushed SECOND (kids[1]) —
    // opposite of the editor's own slot order (slots[0]=degree, slots[1]=radicand).
    // Radicand row ("8") has no caret (cursor sits in it, at its end though —
    // check by content instead of caret since both slots may hold one atom).
    assert(r->kids[0]->kids.size() >= 1);   // radicand "8" (+ caret, cursor is here)
    assert(r->kids[1]->kids.size() == 1);   // degree "3", no caret
}

void test_powerImplicitBaseAndCaretBetween() {
    test::FakeMathCanvas canvas;
    Editor ed;
    ed.insertAtom("x");
    bool ok = ed.insertPower();
    assert(ok);
    ed.insertAtom("2");
    ed.moveLeft();   // exponent row: index 1 -> 0 (still inside the exponent)
    ed.moveLeft();   // exponent row start -> exits the Power node, landing in root
    assert(ed.cursor().row == &ed.root());
    assert(ed.cursor().index == 1);   // right after 'x', at the Power node's slot

    mb::NodePtr root = buildEdRow(canvas, ed.root(), ed.cursor());
    assert(root->kids.size() == 1);   // x popped as the Script's base; Caret consumed too
    mb::Node* sc = root->kids[0].get();
    assert(sc->kind == mb::Node::Script);
    assert(sc->scriptCaret == true);
    assert(sc->kids.size() == 2);
    assert(sc->kids[0]->kind != mb::Node::Placeholder);  // real base (the 'x'), not orphaned
}

void test_powerOrphanBase() {
    test::FakeMathCanvas canvas;
    Editor ed;
    ed.insertAtom("x");
    bool ok = ed.insertPower();
    assert(ok);
    ed.insertAtom("2");
    ed.moveLeft();       // exponent row: index 1 -> 0
    ed.moveLeft();       // exits the Power node -> root, between x and ^ (index 1)
    ed.backspace();      // deletes 'x', leaving the Power node with nothing before it
    assert(ed.cursor().row == &ed.root());
    assert(ed.cursor().index == 0);
    assert(ed.root().items.size() == 1);   // just the orphaned Power node

    mb::NodePtr root = buildEdRow(canvas, ed.root(), ed.cursor());
    assert(root->kids.size() == 2);        // Caret (re-pushed), Script
    assert(root->kids[0]->kind == mb::Node::Caret);
    mb::Node* sc = root->kids[1].get();
    assert(sc->kind == mb::Node::Script);
    assert(sc->scriptCaret == false);      // caret was NOT between base and ^ (no base)
    assert(sc->kids[0]->kind == mb::Node::Placeholder);   // dashed orphan base
}

void test_drawEditorSmoke() {
    test::FakeMathCanvas canvas(/*withFont=*/true);
    Editor ed;
    ed.insertAtom("x");
    ed.insertAtom("+");
    ed.insertAtom("1");
    Rect panel{0, 0, 400.0f, 120.0f};
    drawEditor(canvas, ed, panel);   // must not crash; no return value to assert
}

void test_drawEditorNoFont() {
    test::FakeMathCanvas canvas(/*withFont=*/false);
    Editor ed;
    ed.insertFraction();
    ed.insertAtom("7");
    Rect panel{0, 0, 200.0f, 80.0f};
    drawEditor(canvas, ed, panel);
}

}  // namespace

int main() {
    test_emptyRow();
    test_flatRowCaretAtEnd();
    test_caretMidRow();
    test_matchedParens();
    test_unmatchedParenGhostClose();
    test_fraction();
    test_nthRootDegreeOrderSwap();
    test_powerImplicitBaseAndCaretBetween();
    test_powerOrphanBase();
    test_drawEditorSmoke();
    test_drawEditorNoFont();

    std::puts("editor_layout_test: OK");
    return 0;
}
