#pragma once
#include <array>
#include <cmath>
#include <cstddef>

#include "canvas.hh"
#include "frame_input.hh"
#include "layout.hh"
#include "gray.h"
#include "widgets_gray.h"

enum class Page { Text, Shapes, Widgets, Animation, Math, Hdr, Image, Gestures, TextEdit, Plot };
inline constexpr int kPageCount = 10;

// The tab strip.
//
// It scrolls now, which it did not when there were five tabs and a desktop
// window to put them in. Six tabs at their natural width need about 1200 px,
// and a phone in portrait has around 400 — the old width-clamped scale handled
// that by shrinking every tab until the strip fit, which at phone widths made
// the labels unreadable rather than merely small. So the scale is still clamped
// (to keep a wide window tidy) but only down to a floor, and past that point
// the strip scrolls horizontally instead of shrinking further.
class TopNav {
 public:
    TopNav() {
        for (auto& t : tabs_) t = Button{0.0f, 0.0f, 0.0f, 0.0f};
    }

    // Recomputes tab rects from the current window size — call once per frame
    // before update()/draw(). Mutates each tab's x/y/w/h in place and never
    // reassigns a fresh Button{...}: reconstruction silently resets the private
    // click tracking every frame and makes a completed down-then-up click
    // structurally undetectable (see widgets_gray.h's class comment).
    void updateLayout(Rect windowRect, float uiScaleFactor) {
        const float leadMargin = 20.0f, tabW = 180.0f, tabGap = 10.0f;
        const float naturalStripWidth =
            leadMargin * 2.0f + kPageCount * tabW + (kPageCount - 1) * tabGap;

        // Shrink to fit, but not below the floor — under it, scroll instead.
        float navScale = uiScaleFactor;
        if (naturalStripWidth * navScale > windowRect.w)
            navScale = windowRect.w / naturalStripWidth;
        const float floorScale = 0.55f * uiScaleFactor;
        if (navScale < floorScale) navScale = floorScale;

        Rect strip = dockTop(windowRect, 64.0f * navScale);
        stripRect_ = strip;
        stripWidth_ = naturalStripWidth * navScale;
        maxScroll_  = stripWidth_ - strip.w;
        if (maxScroll_ < 0.0f) maxScroll_ = 0.0f;
        if (scroll_ > maxScroll_) scroll_ = maxScroll_;
        if (scroll_ < 0.0f) scroll_ = 0.0f;

        RowCursor row(strip.x + leadMargin * navScale - scroll_,
                      strip.y + 10.0f * navScale, tabGap * navScale);
        for (int i = 0; i < kPageCount; i++) {
            Rect r = row.next(tabW * navScale, 44.0f * navScale);
            tabs_[(size_t)i].x = r.x; tabs_[(size_t)i].y = r.y;
            tabs_[(size_t)i].w = r.w; tabs_[(size_t)i].h = r.h;
        }
        navScale_ = navScale;
        contentArea_ = windowRect;  // what dockTop left behind, for the pages
    }

    const Rect& contentArea() const { return contentArea_; }

    Page update(const FrameInput& input, Page current) {
        const bool over = stripRect_.contains(input.pointerX, input.pointerY);

        // A wheel over the strip scrolls it — without this the tabs past the
        // right edge are unreachable with a mouse on a narrow window.
        if (maxScroll_ > 0.0f && over && input.wheelDelta != 0.0f) {
            scrollBy(input.wheelDelta * 60.0f);
            dragging_ = false;
        }

        // And a DRAG does the same, which is the half that was missing: ten
        // tabs do not fit a phone, a phone has no wheel, and the tabs past the
        // edge were therefore unreachable on exactly the device that needs the
        // strip to scroll. Same shape as ScrollArea, including the slop — a tap
        // on a tab must select it, not nudge the strip.
        if (input.pointerWentDown && over && maxScroll_ > 0.0f) {
            pressed_  = true;
            dragging_ = false;
            pressX_   = input.pointerX;
            lastX_    = input.pointerX;
        }
        if (pressed_ && input.pointerDown) {
            if (!dragging_ && std::fabs(input.pointerX - pressX_) > kDragSlopPx)
                dragging_ = true;
            if (dragging_) {
                scrollBy(input.pointerX - lastX_);
                lastX_ = input.pointerX;
            }
        }
        const bool wasDragging = dragging_;
        if (pressed_ && !input.pointerDown) { pressed_ = false; dragging_ = false; }

        // A release that ENDED a drag must not also select whatever tab it
        // happened to land on.
        if (wasDragging && !input.pointerDown) return current;

        for (int i = 0; i < kPageCount; i++) {
            if (tabs_[(size_t)i].update(input)) {
                revealTab((Page)i);
                return (Page)i;
            }
        }
        return current;
    }

    // Move the strip by a pointer delta: positive dx drags the content right,
    // which reveals what is to its LEFT — hence the subtraction.
    void scrollBy(float dx) {
        if (maxScroll_ <= 0.0f) return;
        scroll_ -= dx;
        if (scroll_ < 0.0f) scroll_ = 0.0f;
        if (scroll_ > maxScroll_) scroll_ = maxScroll_;
    }

    // Bring a tab fully into view — used when the page changes by a swipe
    // rather than by a tap on a tab that was visible by definition.
    void revealTab(Page p) {
        if (maxScroll_ <= 0.0f) return;
        const Button& t = tabs_[(size_t)p];
        const float margin = 12.0f * navScale_;
        if (t.x < stripRect_.x + margin)
            scrollBy(t.x - (stripRect_.x + margin));
        else if (t.x + t.w > stripRect_.x + stripRect_.w - margin)
            scrollBy((t.x + t.w) - (stripRect_.x + stripRect_.w - margin));
    }

    bool hoversAnyTab(const FrameInput& input) const {
        for (int i = 0; i < kPageCount; i++)
            if (tabs_[(size_t)i].hovered(input)) return true;
        return false;
    }

    void draw(Canvas& c, Page current) const {
        static const char* kLabels[kPageCount] = {
            "Text", "Shapes & Curves", "Widgets", "Animation", "Math",
            "HDR", "Images", "Gestures", "Text entry", "Plot"};
        // Clip to the strip so a scrolled-out tab does not paint over the page.
        c.setClip(stripRect_.x, stripRect_.y, stripRect_.w, stripRect_.h);
        const float labelSize = 18.0f * navScale_;
        for (int i = 0; i < kPageCount; i++) {
            const Button& t = tabs_[(size_t)i];
            if (t.x + t.w < stripRect_.x || t.x > stripRect_.x + stripRect_.w) continue;
            c.rect(t.x, t.y, t.w, t.h, gray(((Page)i == current) ? 0.8f : 0.4f), 6.0f);
            // Canvas::text takes the text-box TOP (baseline = top + size); the
            // renderer this was ported from took a baseline directly.
            c.textCentered(kLabels[i], t.x + t.w * 0.5f,
                           t.y + t.h * 0.5f + 6.0f * navScale_ - labelSize,
                           labelSize, gray(0.05f));
        }
        c.clearClip();
    }

 private:
    std::array<Button, kPageCount> tabs_{
        Button{0,0,0,0}, Button{0,0,0,0}, Button{0,0,0,0}, Button{0,0,0,0},
        Button{0,0,0,0}, Button{0,0,0,0}, Button{0,0,0,0}, Button{0,0,0,0},
        Button{0,0,0,0}, Button{0,0,0,0}};
    static constexpr float kDragSlopPx = 6.0f;

    float navScale_   = 1.0f;
    float scroll_     = 0.0f;
    float pressX_     = 0.0f;
    float lastX_      = 0.0f;
    bool  pressed_    = false;
    bool  dragging_   = false;
    float maxScroll_  = 0.0f;
    float stripWidth_ = 0.0f;
    Rect  stripRect_{0, 0, 0, 0};
    Rect  contentArea_{0, 0, 0, 0};
};
