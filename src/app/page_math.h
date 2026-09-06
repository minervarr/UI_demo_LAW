#pragma once
#include <mathcore/calc.h>

#include "canvas.hh"
#include "frame_input.hh"
#include "../math/math_canvas.h"

class MathPage {
 public:
    // Routes this frame's keyboard input (FrameInput::typedChars +
    // keysWentDown for backspace/arrows) into the live editor's
    // mathcore::calc::Calc and advances the caret blink clock. Scrolling is
    // no longer here: it was wheel-only, so this page could not be read past
    // the fold on a touch screen. The frame loop hands it an already-shifted
    // rect instead — see scroll_area.h.
    void update(float dtSeconds, const FrameInput& input, Rect area);

    void draw(MathCanvas& canvas, Rect area, float uiScaleFactor);

    float contentWidth()  const { return contentWidth_; }
    float contentHeight() const { return contentHeight_; }

  private:
    float contentWidth_  = 0.0f;
    float contentHeight_ = 0.0f;
    // Owns the live, type/backspace/arrow-key-able expression shown below the
    // four static examples. Calc owns its own mathcore editor internally.
    mathcore::calc::Calc liveCalc_;
    float blinkClock_ = 0.0f;  // seconds, wraps every 1.0s; caret shows for the first half
};
