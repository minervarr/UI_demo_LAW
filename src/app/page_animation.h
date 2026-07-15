#pragma once
#include "animated_float.hh"
#include "canvas.hh"
#include "frame_input.hh"
#include "widgets_gray.h"

class AnimationPage {
 public:
    void updateLayout(Rect area, float uiScaleFactor);
    void update(float dtSeconds, const FrameInput& input);
    void draw(Canvas& canvas);

 private:
    Button replayButton_{540, 160, 200, 50};
    AnimatedFloat fade_{0.0f};
    AnimatedFloat moveX_{60.0f};
    float leftX_ = 0.0f, rightX_ = 0.0f;
    float squareY_ = 0.0f, squareSize_ = 120.0f;
    bool laidOut_ = false;
};
