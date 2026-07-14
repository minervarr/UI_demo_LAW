#include "input_state.h"
#include <cassert>
#include <cstdio>

int main() {
    InputState s{};

    // Frame 1: mouse goes down
    s.beginFrame();
    s.mouseDown = true;
    s.mouseWentDown = true;
    assert(s.mouseWentDown == true);

    // Frame 2: mouse stays down — went-down edge must clear
    s.beginFrame();
    assert(s.mouseWentDown == false);
    assert(s.mouseDown == true);

    // Frame 3: mouse released
    s.mouseDown = false;
    s.mouseWentUp = true;
    assert(s.mouseWentUp == true);

    // Frame 4: went-up edge must clear even though mouseDown stays false
    s.beginFrame();
    assert(s.mouseWentUp == false);
    assert(s.mouseDown == false);

    // --- Keyboard: typed-character queue ---

    // Frame 5: a single WM_CHAR arrives ('a') and is visible this frame.
    s.beginFrame();
    s.typedChars += 'a';
    assert(s.typedChars == "a");

    // Frame 6: nothing new typed — queue must be cleared by beginFrame().
    s.beginFrame();
    assert(s.typedChars.empty());

    // Frame 7: multiple WM_CHAR messages arrive before the next beginFrame()
    // (fast typing/paste) — all must be preserved, in arrival order, not
    // dropped or overwritten.
    s.typedChars += 'x';
    s.typedChars += 'y';
    s.typedChars += 'z';
    assert(s.typedChars == "xyz");

    // Frame 8: queue clears again even after a multi-char frame.
    s.beginFrame();
    assert(s.typedChars.empty());

    // --- Keyboard: special-key edge flags ---

    // Frame 9: left/right/up/down/backspace all go true on their press frame.
    s.beginFrame();
    s.keyLeftPressed = true;
    s.keyRightPressed = true;
    s.keyUpPressed = true;
    s.keyDownPressed = true;
    s.keyBackspacePressed = true;
    assert(s.keyLeftPressed == true);
    assert(s.keyRightPressed == true);
    assert(s.keyUpPressed == true);
    assert(s.keyDownPressed == true);
    assert(s.keyBackspacePressed == true);

    // Frame 10: every edge flag must clear even though none were re-pressed.
    s.beginFrame();
    assert(s.keyLeftPressed == false);
    assert(s.keyRightPressed == false);
    assert(s.keyUpPressed == false);
    assert(s.keyDownPressed == false);
    assert(s.keyBackspacePressed == false);

    // Frame 11: repeated presses of the same key across multiple frames each
    // register their own edge — not stuck permanently true (auto-repeat
    // WM_KEYDOWN semantics) and not stuck permanently false after the first
    // press (queue-clearing bug).
    s.beginFrame();
    s.keyBackspacePressed = true;
    assert(s.keyBackspacePressed == true);

    s.beginFrame();
    assert(s.keyBackspacePressed == false);  // cleared between presses

    s.beginFrame();
    s.keyBackspacePressed = true;
    assert(s.keyBackspacePressed == true);  // fires again on the next press

    s.beginFrame();
    assert(s.keyBackspacePressed == false);  // and clears again afterward

    // --- Mouse wheel ---

    // Frame 12: several WM_MOUSEWHEEL messages between frames accumulate
    // (in notches), including mixed directions.
    s.beginFrame();
    s.wheelDelta += 1.0f;
    s.wheelDelta += 1.0f;
    s.wheelDelta += -0.5f;
    assert(s.wheelDelta == 1.5f);

    // Frame 13: beginFrame() clears the accumulator like the edge flags.
    s.beginFrame();
    assert(s.wheelDelta == 0.0f);

    printf("input_state_test: OK\n");
    return 0;
}
