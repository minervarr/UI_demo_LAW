#include "page_animation.h"
#include <cmath>

void AnimationPage::updateLayout(Rect area, float uiScaleFactor) {
    // Mutate replayButton_'s x/y/w/h in place — see nav.h's updateLayout for
    // why reassigning a fresh `Button{...}` here would silently reset its
    // private click-tracking state every frame and make Replay unclickable.
    Rect r = centerIn(area, 200.0f * uiScaleFactor, 50.0f * uiScaleFactor);
    replayButton_.x = r.x; replayButton_.y = area.y + 60.0f * uiScaleFactor;
    replayButton_.w = r.w; replayButton_.h = r.h;
    float newLeftX = area.x + 60.0f * uiScaleFactor;
    float newRightX = area.x + area.w - 60.0f * uiScaleFactor - 120.0f * uiScaleFactor;
    squareY_ = area.y + 240.0f * uiScaleFactor;
    squareSize_ = 120.0f * uiScaleFactor;

    // updateLayout() runs every frame (not just on an actual resize), so
    // re-targeting must only happen when the endpoints genuinely moved.
    // Without this guard, the brief's original "if isAnimating(), retarget
    // with a 0.01s duration" check would fire on every single frame of any
    // in-flight animation (isAnimating() stays true for ~1.5s), collapsing
    // the intended 1.5s slide into a snap after just two frames.
    bool endpointsChanged = laidOut_ &&
        (std::fabs(newLeftX - leftX_) > 0.01f || std::fabs(newRightX - rightX_) > 0.01f);
    leftX_ = newLeftX;
    rightX_ = newRightX;

    if (!laidOut_) {
        // First layout pass: snap the square to its start position and kick
        // off the original intro fade/slide animation now that we know the
        // real (window-size-derived) endpoints, instead of the hardcoded
        // 60.0f/700.0f literals the old constructor used.
        moveX_ = AnimatedFloat(leftX_);
        fade_ = AnimatedFloat(0.0f);
        fade_.set(1.0f, 1.5f, easeInOutCubic);
        moveX_.set(rightX_, 1.5f, easeInOutCubic);
        laidOut_ = true;
    } else if (endpointsChanged) {
        if (fade_.isAnimating() || moveX_.isAnimating()) {
            // Re-target in-flight animations to the rescaled endpoints so a
            // resize mid-animation doesn't leave the square heading for a
            // stale coordinate.
            moveX_.set(moveX_.value() > (leftX_ + rightX_) * 0.5f ? rightX_ : leftX_, 0.01f);
        } else {
            // At rest the square must re-anchor too: without this a
            // maximize/restore left it parked at the OLD window's pixel
            // position (observed as a wildly wrong left margin until the
            // next Replay).
            moveX_ = AnimatedFloat(
                moveX_.value() > (leftX_ + rightX_) * 0.5f ? rightX_ : leftX_);
        }
    }
}

void AnimationPage::update(float dtSeconds, const InputState& input) {
    if (replayButton_.update(input)) {
        fade_.set(fade_.value() > 0.5f ? 0.1f : 1.0f, 1.5f, easeInOutCubic);
        moveX_.set(moveX_.value() > (leftX_ + rightX_) * 0.5f ? leftX_ : rightX_, 1.5f, easeInOutCubic);
    }
    fade_.update(dtSeconds);
    moveX_.update(dtSeconds);
}

void AnimationPage::draw(PrimitiveBatch& batch, TextRenderer& text) {
    replayButton_.draw(batch, text, "Replay");
    batch.pushRoundedRect(moveX_.value(), squareY_, squareSize_, squareSize_, 16.0f, 0.9f, fade_.value());
}
