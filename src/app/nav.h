#pragma once
#include <array>

#include "canvas.hh"
#include "frame_input.hh"
#include "layout.hh"
#include "gray.h"
#include "widgets_gray.h"

enum class Page { Text, Shapes, Widgets, Animation, Math };

class TopNav {
 public:
    TopNav()
        : tabs_{Button{20.0f, 20.0f, 180.0f, 44.0f},
                Button{210.0f, 20.0f, 180.0f, 44.0f},
                Button{400.0f, 20.0f, 180.0f, 44.0f},
                Button{590.0f, 20.0f, 180.0f, 44.0f},
                Button{780.0f, 20.0f, 180.0f, 44.0f}} {}

    // Recomputes tab rects from the current window size — call once per
    // frame before update()/draw(). Mutates tabs_[i]'s fields in place, never
    // reassigns a fresh Button{...} (see widgets_gray.h's class comment).
    void updateLayout(Rect windowRect, float uiScaleFactor) {
        // UiScale is height-derived and uncapped above (see vk_canvas
        // layout.hh), so a tall window can scale the tab strip's natural
        // width past the window's actual width. Clamp the nav's own scale to
        // whatever additionally fits the real width so tabs shrink together
        // instead of clipping off-screen.
        const float leadMargin = 20.0f, tabW = 180.0f, tabGap = 10.0f;
        const float naturalStripWidth = leadMargin + 5.0f * tabW + 4.0f * tabGap;
        float navScale = uiScaleFactor;
        if (naturalStripWidth * navScale > windowRect.w) {
            navScale = windowRect.w / naturalStripWidth;
        }
        Rect strip = dockTop(windowRect, 64.0f * navScale);
        RowCursor row(strip.x + leadMargin * navScale, strip.y + 10.0f * navScale,
                      tabGap * navScale);
        for (int i = 0; i < 5; i++) {
            Rect r = row.next(tabW * navScale, 44.0f * navScale);
            tabs_[i].x = r.x; tabs_[i].y = r.y; tabs_[i].w = r.w; tabs_[i].h = r.h;
        }
        navScale_ = navScale;
        contentArea_ = windowRect;  // what dockTop left behind, for pages to use
    }

    const Rect& contentArea() const { return contentArea_; }

    Page update(const FrameInput& input, Page current) {
        for (int i = 0; i < 5; i++) {
            if (tabs_[i].update(input)) return (Page)i;
        }
        return current;
    }

    bool hoversAnyTab(const FrameInput& input) const {
        for (int i = 0; i < 5; i++) {
            if (tabs_[i].hovered(input)) return true;
        }
        return false;
    }

    void draw(Canvas& c, Page current) const {
        const char* labels[5] = {"Text", "Shapes & Curves", "Widgets", "Animation", "Math"};
        // Label size follows the same width-clamped navScale_ as the tab
        // rects — a fixed size would overflow a tab the clamp shrank.
        float labelSize = 18.0f * navScale_;
        for (int i = 0; i < 5; i++) {
            c.rect(tabs_[i].x, tabs_[i].y, tabs_[i].w, tabs_[i].h,
                   gray(((Page)i == current) ? 0.8f : 0.4f), 6.0f);
            // Old renderer took a baseline (y + h/2 + 6*scale); Canvas takes
            // the text-box top (baseline - size).
            c.textCentered(labels[i], tabs_[i].x + tabs_[i].w * 0.5f,
                           tabs_[i].y + tabs_[i].h * 0.5f + 6.0f * navScale_ - labelSize,
                           labelSize, gray(0.05f));
        }
    }

 private:
    std::array<Button, 5> tabs_;
    float navScale_ = 1.0f;
    Rect contentArea_{0, 0, 0, 0};
};
