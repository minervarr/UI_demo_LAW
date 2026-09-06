#pragma once
#include <algorithm>
#include <cmath>

#include "canvas.hh"
#include "frame_input.hh"

#include "gray.h"

// Reaching content that does not fit.
//
// This exists because the demo had scrolling on two pages, by wheel only, and
// a wheel is the one input a phone does not have — so on the platform where
// content is most likely to overflow, nothing could be reached at all. It
// scrolls on BOTH axes and takes drags as well as the wheel, which is the same
// gesture a finger and a mouse both produce.
//
// It is deliberately not a widget. It owns no rectangle of its own and draws
// no background: a page lays itself out at its natural size, tells this what
// that size was, and adds offsetX()/offsetY() to every coordinate. What the
// page draws is unchanged; where it lands moves.
//
// ── The one thing to get right when using it ────────────────────────────────
//
// A drag that starts on a slider must move the slider, not the page. This
// cannot decide that for itself — only the page knows what its own widgets
// are under the pointer — so update() takes `pointerClaimed`. Pass true when a
// widget wants the press. Getting it wrong is not subtle: sliders stop
// working, or the page cannot be dragged.
class ScrollArea {
 public:
    void setViewport(Rect v) { view_ = v; }

    // The size the page WOULD occupy if nothing clipped it. Measured by the
    // page during layout, so it is exact rather than assumed — and one frame
    // stale after a resize, which is harmless because the clamp below runs
    // every frame anyway.
    void setContent(float w, float h) { contentW_ = w; contentH_ = h; }

    float maxX() const { return std::max(0.0f, contentW_ - view_.w); }
    float maxY() const { return std::max(0.0f, contentH_ - view_.h); }
    bool  overflowsX() const { return maxX() > 0.5f; }
    bool  overflowsY() const { return maxY() > 0.5f; }

    // Add these to the page's own coordinates. Negative as they grow, because
    // scrolling down moves content up.
    float offsetX() const { return -x_; }
    float offsetY() const { return -y_; }

    void update(const FrameInput& in, float dt, bool pointerClaimed) {
        const bool over = view_.contains(in.pointerX, in.pointerY);

        // Wheel: vertical normally, horizontal when the content only overflows
        // sideways — a plain wheel is the only pointing device with no second
        // axis, and a page that scrolls horizontally would otherwise be stuck.
        if (over && in.wheelDelta != 0.0f) {
            if (overflowsY()) y_ -= in.wheelDelta * kWheelStepPx;
            else if (overflowsX()) x_ -= in.wheelDelta * kWheelStepPx;
            vy_ = vx_ = 0.0f;
        }

        // Drag. Starts only on a press this page did not claim for a widget,
        // and only after the pointer has moved far enough to not be a tap —
        // without that threshold every click on empty space nudges the page.
        if (in.pointerWentDown && over && !pointerClaimed) {
            pressed_  = true;
            dragging_ = false;
            pressX_   = in.pointerX;
            pressY_   = in.pointerY;
            lastX_    = in.pointerX;
            lastY_    = in.pointerY;
            vx_ = vy_ = 0.0f;
        }
        if (pressed_ && in.pointerDown) {
            const float dx = in.pointerX - pressX_;
            const float dy = in.pointerY - pressY_;
            if (!dragging_ && (std::fabs(dx) > kDragSlopPx || std::fabs(dy) > kDragSlopPx))
                dragging_ = true;
            if (dragging_) {
                const float stepX = in.pointerX - lastX_;
                const float stepY = in.pointerY - lastY_;
                x_ -= stepX;
                y_ -= stepY;
                // Velocity for the fling, smoothed so one jittery sample does
                // not throw the whole gesture.
                if (dt > 0.0f) {
                    vx_ = 0.7f * vx_ + 0.3f * (-stepX / dt);
                    vy_ = 0.7f * vy_ + 0.3f * (-stepY / dt);
                }
                lastX_ = in.pointerX;
                lastY_ = in.pointerY;
            }
        }
        if (pressed_ && !in.pointerDown) {
            pressed_  = false;
            dragging_ = false;
        }

        // Fling, with a friction that is per-second rather than per-frame:
        // tying it to the frame rate makes the same flick travel twice as far
        // at 120 Hz as at 60.
        if (!pressed_ && (std::fabs(vx_) > 1.0f || std::fabs(vy_) > 1.0f)) {
            x_ += vx_ * dt;
            y_ += vy_ * dt;
            const float damp = std::pow(kFlingFriction, dt);
            vx_ *= damp;
            vy_ *= damp;
        }

        clamp();
    }

    void reset() { x_ = y_ = vx_ = vy_ = 0.0f; pressed_ = dragging_ = false; }

    // A thin bar per overflowing axis, inside the viewport's trailing edge.
    // Drawn only when there IS something off-screen, so it doubles as the
    // answer to "is there more below?" — which on a touch screen is the only
    // clue there is, with no wheel and no visible edge to hint at it.
    void drawBars(Canvas& c, float scale) const {
        const float t = std::max(3.0f, 4.0f * scale);
        if (overflowsY()) {
            const float trackH = view_.h;
            const float thumbH = std::max(24.0f * scale, trackH * (view_.h / contentH_));
            const float t01    = maxY() > 0.0f ? (y_ / maxY()) : 0.0f;
            const float thumbY = view_.y + t01 * (trackH - thumbH);
            c.rect(view_.x + view_.w - t - 2.0f * scale, view_.y, t, trackH, gray(0.42f), t * 0.5f);
            c.rect(view_.x + view_.w - t - 2.0f * scale, thumbY, t, thumbH, gray(0.85f), t * 0.5f);
        }
        if (overflowsX()) {
            const float trackW = view_.w;
            const float thumbW = std::max(24.0f * scale, trackW * (view_.w / contentW_));
            const float t01    = maxX() > 0.0f ? (x_ / maxX()) : 0.0f;
            const float thumbX = view_.x + t01 * (trackW - thumbW);
            c.rect(view_.x, view_.y + view_.h - t - 2.0f * scale, trackW, t, gray(0.42f), t * 0.5f);
            c.rect(thumbX, view_.y + view_.h - t - 2.0f * scale, thumbW, t, gray(0.85f), t * 0.5f);
        }
    }

 private:
    static constexpr float kWheelStepPx    = 60.0f;
    static constexpr float kDragSlopPx     = 6.0f;
    static constexpr float kFlingFriction  = 0.06f;   // per second

    void clamp() {
        const float mx = maxX(), my = maxY();
        if (x_ < 0.0f)  { x_ = 0.0f;  vx_ = 0.0f; }
        if (x_ > mx)    { x_ = mx;    vx_ = 0.0f; }
        if (y_ < 0.0f)  { y_ = 0.0f;  vy_ = 0.0f; }
        if (y_ > my)    { y_ = my;    vy_ = 0.0f; }
    }

    Rect  view_{0, 0, 0, 0};
    float contentW_ = 0.0f, contentH_ = 0.0f;
    float x_ = 0.0f, y_ = 0.0f;
    float vx_ = 0.0f, vy_ = 0.0f;
    float pressX_ = 0.0f, pressY_ = 0.0f;
    float lastX_ = 0.0f, lastY_ = 0.0f;
    bool  pressed_  = false;
    bool  dragging_ = false;
};
