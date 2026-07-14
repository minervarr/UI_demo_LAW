#pragma once
#include "../math/math_canvas.h"
#include "../platform/input_state.h"
#include "../ui/layout.h"
#include <mathcore/calc.h>

class MathPage {
 public:
    // Routes this frame's keyboard input (InputState::typedChars +
    // keyLeft/Right/Up/Down/BackspacePressed) into the live editor's
    // mathcore::calc::Calc, advances the caret blink clock, and applies the
    // mouse wheel to the page scroll offset (clamped against the content
    // height measured by the previous draw()). Call once per frame before
    // draw(), same update/draw split as AnimationPage.
    void update(float dtSeconds, const InputState& input, Rect area);

    void draw(MathCanvas& canvas, Rect area, float uiScaleFactor);

 private:
    float scrollY_ = 0.0f;
    float contentHeight_ = 0.0f;
    // Owns the live, type/backspace/arrow-key-able expression shown in the
    // 5th row below the 4 static CachedExpression examples (Task 9). Calc
    // owns its own mathcore::calcedit::Editor internally.
    mathcore::calc::Calc liveCalc_;
    float blinkClock_ = 0.0f;  // seconds, wraps every 1.0s; caret shows for the first half
};
