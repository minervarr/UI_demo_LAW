#include "input_state.h"

void InputState::beginFrame() {
    mouseWentDown = false;
    mouseWentUp = false;
    typedChars.clear();
    keyLeftPressed = false;
    keyRightPressed = false;
    keyUpPressed = false;
    keyDownPressed = false;
    keyBackspacePressed = false;
    wheelDelta = 0.0f;
}
