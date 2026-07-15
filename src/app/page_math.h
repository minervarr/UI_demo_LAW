#pragma once
#include <mathcore/calc.h>

#include "canvas.hh"
#include "frame_input.hh"
#include "../math/math_canvas.h"

class MathPage {
 public:
    // Routes this frame's keyboard input (FrameInput::typedChars +
    // keysWentDown for backspace/arrows) into the live editor's
    // mathcore::calc::Calc, advances the caret blink clock, and applies the
    // mouse wheel to the scroll offset (clamped against the content height
    // measured by the previous draw()). Call once per frame before draw().
    void update(float dtSeconds, const FrameInput& input, Rect area);

    void draw(MathCanvas& canvas, Rect area, float uiScaleFactor);

 private:
    float scrollY_ = 0.0f;
    float contentHeight_ = 0.0f;
    // Owns the live, type/backspace/arrow-key-able expression shown below the
    // four static examples. Calc owns its own mathcore editor internally.
    mathcore::calc::Calc liveCalc_;
    float blinkClock_ = 0.0f;  // seconds, wraps every 1.0s; caret shows for the first half
};
