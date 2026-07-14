#pragma once
#include "../anim/animated_float.h"
#include "../gfx/primitives.h"
#include "../text/text_renderer.h"
#include "../ui/widgets.h"
#include "../ui/layout.h"
#include "../platform/input_state.h"

class AnimationPage {
 public:
    void updateLayout(Rect area, float uiScaleFactor);
    void update(float dtSeconds, const InputState& input);
    void draw(PrimitiveBatch& batch, TextRenderer& text);

 private:
    Button replayButton_{60, 140, 200, 50};
    AnimatedFloat fade_{0.0f};
    AnimatedFloat moveX_{60.0f};
    float leftX_ = 60.0f, rightX_ = 700.0f, squareY_ = 240.0f, squareSize_ = 120.0f;
    bool laidOut_ = false;
};
