#include "page_animation.h"

#include <cmath>

#include "layout.hh"
#include "gray.h"

void AnimationPage::updateLayout(Rect area, float uiScaleFactor) {
    // Mutate replayButton_'s fields in place — see widgets_gray.h's class
    // comment for why reassigning a fresh Button{...} would silently reset
    // its click-tracking state every frame.
    Rect r = centerIn(area, 200.0f * uiScaleFactor, 50.0f * uiScaleFactor);
    replayButton_.x = r.x; replayButton_.y = area.y + 60.0f * uiScaleFactor;
    replayButton_.w = r.w; replayButton_.h = r.h;
    float newLeftX = area.x + 60.0f * uiScaleFactor;
    float newRightX = area.x + area.w - 60.0f * uiScaleFactor - 120.0f * uiScaleFactor;
    squareY_ = area.y + 240.0f * uiScaleFactor;
    squareSize_ = 120.0f * uiScaleFactor;

    // updateLayout() runs every frame (not just on a resize), so re-targeting
    // must only happen when the endpoints genuinely moved — otherwise any
    // in-flight animation would be re-targeted (and effectively snapped)
    // every frame.
    bool endpointsChanged = laidOut_ &&
        (std::fabs(newLeftX - leftX_) > 0.01f || std::fabs(newRightX - rightX_) > 0.01f);
    leftX_ = newLeftX;
    rightX_ = newRightX;

    if (!laidOut_) {
        // First layout pass: snap the square to its start position and kick
        // off the intro fade/slide now that the real window-size-derived
        // endpoints are known.
        moveX_ = AnimatedFloat(leftX_);
        fade_ = AnimatedFloat(0.0f);
        fade_.set(1.0f, 1.5f, easeInOutCubic);
        moveX_.set(rightX_, 1.5f, easeInOutCubic);
        laidOut_ = true;
    } else if (endpointsChanged) {
        if (fade_.isAnimating() || moveX_.isAnimating()) {
            // Re-target in-flight animations to the rescaled endpoints so a
            // resize mid-animation doesn't head for a stale coordinate.
            moveX_.set(moveX_.value() > (leftX_ + rightX_) * 0.5f ? rightX_ : leftX_, 0.01f);
        } else {
            // At rest the square must re-anchor too: without this a
            // maximize/restore leaves it parked at the OLD window's pixels.
            moveX_ = AnimatedFloat(
                moveX_.value() > (leftX_ + rightX_) * 0.5f ? rightX_ : leftX_);
        }
    }
}

void AnimationPage::update(float dtSeconds, const FrameInput& input) {
    if (replayButton_.update(input)) {
        fade_.set(fade_.value() > 0.5f ? 0.1f : 1.0f, 1.5f, easeInOutCubic);
        moveX_.set(moveX_.value() > (leftX_ + rightX_) * 0.5f ? leftX_ : rightX_, 1.5f, easeInOutCubic);
    }
    fade_.update(dtSeconds);
    moveX_.update(dtSeconds);
}

void AnimationPage::draw(Canvas& canvas) {
    replayButton_.draw(canvas, "Replay");
    canvas.rect(moveX_.value(), squareY_, squareSize_, squareSize_,
                gray(0.9f, fade_.value()), 16.0f);
}
