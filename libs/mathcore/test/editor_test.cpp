// libs/mathcore/test/editor_test.cpp
#include <mathcore/editor.h>
#include <cassert>
#include <cstdio>

using namespace mathcore::calcedit;

static void insertDigits(Editor& e, const std::string& s) {
    for (char c : s) {
        bool ok = e.insertAtom(std::string(1, c));
        (void)ok;
    }
}

static void test_insertFraction() {
    Editor e;
    bool ok = e.insertFraction();
    assert(ok);
    // Tree shape: root has exactly one node, a Frac with 2 slots, both empty.
    assert(e.root().items.size() == 1);
    Node* frac = e.root().items[0].get();
    assert(frac->kind == Node::Frac);
    assert(frac->slots.size() == 2);
    assert(frac->slots[0].items.empty());
    assert(frac->slots[1].items.empty());
    // Cursor moved into the numerator (slot 0), index 0.
    assert(e.cursor().row == &frac->slots[0]);
    assert(e.cursor().index == 0);

    // Type "8" into numerator, moveRight into denominator, type "5".
    insertDigits(e, "8");
    assert(e.cursor().row == &frac->slots[0]);
    assert(e.cursor().index == 1);
    e.moveRight();  // end of numerator row -> crosses into denominator start
    assert(e.cursor().row == &frac->slots[1]);
    assert(e.cursor().index == 0);
    insertDigits(e, "5");
    assert(e.linearize() == "(8)/(5)");
}

static void test_insertSqrt() {
    Editor e;
    bool ok = e.insertSqrt();
    assert(ok);
    assert(e.root().items.size() == 1);
    Node* sq = e.root().items[0].get();
    assert(sq->kind == Node::Sqrt);
    assert(sq->slots.size() == 1);
    assert(e.cursor().row == &sq->slots[0]);
    assert(e.cursor().index == 0);

    insertDigits(e, "9");
    assert(e.linearize() == "sqrt(9)");
}

static void test_insertNthRoot() {
    Editor e;
    bool ok = e.insertNthRoot();
    assert(ok);
    assert(e.root().items.size() == 1);
    Node* nr = e.root().items[0].get();
    assert(nr->kind == Node::NthRoot);
    assert(nr->slots.size() == 2);
    // Cursor starts in slot 0: the index (degree), typed first.
    assert(e.cursor().row == &nr->slots[0]);
    assert(e.cursor().index == 0);

    insertDigits(e, "3");  // index = 3
    e.moveRight();         // index -> radicand
    assert(e.cursor().row == &nr->slots[1]);
    insertDigits(e, "8");  // radicand = 8

    // linearize: radicand ^ (1/(index)) -> "(8)^(1/(3))"
    assert(e.linearize() == "(8)^(1/(3))");
}

static void test_insertPower() {
    Editor e;
    insertDigits(e, "2");
    bool ok = e.insertPower();
    assert(ok);
    assert(e.root().items.size() == 2);  // Atom "2", Power
    Node* pw = e.root().items[1].get();
    assert(pw->kind == Node::Power);
    assert(pw->slots.size() == 1);
    assert(e.cursor().row == &pw->slots[0]);
    assert(e.cursor().index == 0);

    insertDigits(e, "5");
    assert(e.linearize() == "2^(5)");
}

static void test_insertPower_guard() {
    // Fails at row start: nothing to raise.
    {
        Editor e;
        assert(!e.insertPower());
        assert(e.root().items.empty());
    }
    // Fails right after an operator.
    {
        Editor e;
        insertDigits(e, "2");
        bool okOp = e.insertAtom("+");
        assert(okOp);
        assert(!e.insertPower());
    }
    // Fails right after "(".
    {
        Editor e;
        bool okParen = e.insertAtom("(");
        assert(okParen);
        assert(!e.insertPower());
    }
    // Fails right after "sin(" (an Atom ending in '(').
    {
        Editor e;
        bool okSin = e.insertAtom("sin(");
        assert(okSin);
        assert(!e.insertPower());
    }
    // Succeeds right after a value-ending atom.
    {
        Editor e;
        insertDigits(e, "9");
        assert(e.insertPower());
    }
    // Succeeds right after a template (ends in ')').
    {
        Editor e;
        assert(e.insertSqrt());
        insertDigits(e, "4");
        e.moveRight();  // exit sqrt back to root, after the Sqrt node
        assert(e.cursorAtRoot());
        assert(e.insertPower());
    }
}

static void test_backspace_unwrap_at_slot_start() {
    // "8/5" backspaced at the start of "8" -> unwraps the Frac, splicing
    // "8" and "5" back into the root row, cursor lands before the splice
    // (so the whole thing reads "85").
    Editor e;
    e.insertFraction();
    Node* frac = e.root().items[0].get();
    insertDigits(e, "8");
    e.moveRight();
    insertDigits(e, "5");
    // Cursor is now at the end of the denominator "5" (slot 1, index 1).
    assert(e.cursor().row == &frac->slots[1]);
    assert(e.cursor().index == 1);
    e.moveLeft();  // start of denominator "5" (index 0)
    assert(e.cursor().index == 0);
    e.moveLeft();  // crosses den->num (leftSiblingSlot), landing at end of numerator "8"
    assert(e.cursor().row == &frac->slots[0]);
    assert(e.cursor().index == 1);
    e.moveLeft();  // now at the start of the numerator, index 0
    assert(e.cursor().row == &frac->slots[0]);
    assert(e.cursor().index == 0);

    e.backspace();  // unwrap: splice "8","5" into root before the Frac's old position
    assert(e.linearize() == "85");
    assert(e.cursor().row == &e.root());
    assert(e.cursor().index == 0);
}

static void test_backspace_deletes_empty_template_in_one_press() {
    Editor e;
    e.insertFraction();
    // Both slots empty; cursor is inside numerator, index 0 (start of first slot).
    e.backspace();
    assert(e.empty());
    assert(e.cursorAtRoot());
    assert(e.cursor().index == 0);
}

static void test_backspace_deletes_atom() {
    Editor e;
    insertDigits(e, "12");
    e.backspace();
    assert(e.linearize() == "1");
    e.backspace();
    assert(e.linearize() == "");
    assert(e.empty());
}

static void test_backspace_steps_into_filled_template_from_right_edge() {
    // Cursor sits right after a filled Frac (root level); one backspace steps
    // INTO the template (lands at the end of the last slot) rather than
    // deleting the whole thing.
    Editor e;
    e.insertFraction();
    Node* frac = e.root().items[0].get();
    insertDigits(e, "8");
    e.moveRight();
    insertDigits(e, "5");
    e.moveRight();  // exit the Frac to the root, cursor after it
    assert(e.cursorAtRoot());
    assert(e.cursor().index == 1);

    e.backspace();  // steps into slot 1 (denominator) end, does NOT delete anything yet
    assert(e.cursor().row == &frac->slots[1]);
    assert(e.cursor().index == 1);
    assert(e.linearize() == "(8)/(5)");  // nothing consumed yet

    e.backspace();  // now deletes the "5" atom
    assert(e.linearize() == "(8)/()");
}

static void test_cross_slot_navigation_moveRight_num_to_den() {
    Editor e;
    e.insertFraction();
    Node* frac = e.root().items[0].get();
    insertDigits(e, "3");
    assert(e.cursor().row == &frac->slots[0]);
    assert(e.cursor().index == 1);
    e.moveRight();  // at end of numerator row -> crosses into denominator start
    assert(e.cursor().row == &frac->slots[1]);
    assert(e.cursor().index == 0);
}

static void test_moveUp_moveDown_frac() {
    Editor e;
    e.insertFraction();
    Node* frac = e.root().items[0].get();
    insertDigits(e, "1");
    e.moveRight();
    insertDigits(e, "22");
    // Cursor at end of denominator (index 2).
    assert(e.cursor().row == &frac->slots[1]);
    assert(e.cursor().index == 2);

    e.moveUp();
    assert(e.cursor().row == &frac->slots[0]);
    // Numerator has only 1 item, so index clamps to 1.
    assert(e.cursor().index == 1);

    e.moveDown();
    assert(e.cursor().row == &frac->slots[1]);
    // Denominator has 2 items; clamp(min(1,2)) == 1.
    assert(e.cursor().index == 1);
}

static void test_linearize_roundtrips() {
    {
        Editor e;
        e.insertFraction();
        insertDigits(e, "7");
        e.moveRight();
        insertDigits(e, "9");
        assert(e.linearize() == "(7)/(9)");
    }
    {
        Editor e;
        e.insertSqrt();
        insertDigits(e, "2");
        assert(e.linearize() == "sqrt(2)");
    }
    {
        Editor e;
        e.insertNthRoot();
        insertDigits(e, "3");
        e.moveRight();
        insertDigits(e, "27");
        assert(e.linearize() == "(27)^(1/(3))");
    }
    {
        Editor e;
        insertDigits(e, "2");
        e.insertPower();
        insertDigits(e, "10");
        assert(e.linearize() == "2^(10)");
    }
}

static void test_loadString_and_isLoneZero() {
    Editor e;
    e.loadString("0");
    assert(e.isLoneZero());
    e.loadString("sin(x)+1");
    assert(e.linearize() == "sin(x)+1");
    assert(!e.isLoneZero());
}

int main() {
    test_insertFraction();
    test_insertSqrt();
    test_insertNthRoot();
    test_insertPower();
    test_insertPower_guard();
    test_backspace_unwrap_at_slot_start();
    test_backspace_deletes_empty_template_in_one_press();
    test_backspace_deletes_atom();
    test_backspace_steps_into_filled_template_from_right_edge();
    test_cross_slot_navigation_moveRight_num_to_den();
    test_moveUp_moveDown_frac();
    test_linearize_roundtrips();
    test_loadString_and_isLoneZero();

    printf("editor_test: OK\n");
    return 0;
}
