#pragma once
#include <array>
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
    bool scrollable() const { return maxScroll_ > 0.5f; }

    Page update(const FrameInput& input, Page current) {
        // A wheel over the strip scrolls it. Without this the tabs past the
        // right edge are unreachable with a mouse on a narrow window.
        if (maxScroll_ > 0.0f && stripRect_.contains(input.pointerX, input.pointerY) &&
            input.wheelDelta != 0.0f) {
            scroll_ -= input.wheelDelta * 60.0f;
            if (scroll_ < 0.0f) scroll_ = 0.0f;
            if (scroll_ > maxScroll_) scroll_ = maxScroll_;
        }
        for (int i = 0; i < kPageCount; i++) {
            if (tabs_[(size_t)i].update(input)) return (Page)i;
        }
        return current;
    }

    // Drag the strip sideways — the touch equivalent of the wheel above.
    void scrollBy(float dx) {
        if (maxScroll_ <= 0.0f) return;
        scroll_ -= dx;
        if (scroll_ < 0.0f) scroll_ = 0.0f;
        if (scroll_ > maxScroll_) scroll_ = maxScroll_;
    }
    bool stripContains(float x, float y) const { return stripRect_.contains(x, y); }

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
    float navScale_   = 1.0f;
    float scroll_     = 0.0f;
    float maxScroll_  = 0.0f;
    float stripWidth_ = 0.0f;
    Rect  stripRect_{0, 0, 0, 0};
    Rect  contentArea_{0, 0, 0, 0};
};
