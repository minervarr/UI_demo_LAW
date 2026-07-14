#pragma once
#include <string>

struct InputState {
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool mouseDown = false;      // held state, persists across frames
    bool mouseWentDown = false;  // true only on the frame the button was pressed
    bool mouseWentUp = false;    // true only on the frame the button was released

    // Keyboard text entry (WM_CHAR): printable characters typed this frame,
    // in arrival order. Matches mouseWentDown/Up's edge-triggered style —
    // cleared every beginFrame() — but as a small queue rather than a single
    // flag, since several WM_CHAR messages can arrive between two frames
    // (fast typing/paste). Only ASCII printable chars are appended (control
    // chars like backspace/enter are handled via the WM_KEYDOWN flags below
    // instead, so they don't show up twice).
    std::string typedChars;

    // Special-key edge flags (true only on the frame(s) a WM_KEYDOWN for that
    // key arrived — same "went down" semantics as mouseWentDown, so a held
    // key auto-repeats one edge per repeated WM_KEYDOWN rather than latching
    // permanently true).
    bool keyLeftPressed = false;
    bool keyRightPressed = false;
    bool keyUpPressed = false;
    bool keyDownPressed = false;
    bool keyBackspacePressed = false;

    // Mouse wheel movement this frame, in notches (positive = away from the
    // user). Accumulated because several WM_MOUSEWHEEL messages can arrive
    // between two frames; cleared every beginFrame() like the edge flags.
    float wheelDelta = 0.0f;

    // Clears the per-frame edge flags/queues. Call once per frame before
    // pumping Win32 messages (WM_LBUTTONDOWN/UP, WM_CHAR, WM_KEYDOWN set them
    // for that frame).
    void beginFrame();
};
