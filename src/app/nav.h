#pragma once
#include "../platform/input_state.h"
#include "../gfx/primitives.h"
#include "../text/text_renderer.h"
#include "../ui/widgets.h"
#include "../ui/layout.h"
#include <array>

enum class Page { Text, Shapes, Widgets, Animation, Math };

class TopNav {
 public:
    // Button (Task 7) has only a 4-float constructor, no default constructor,
    // so tabs_ (a std::array<Button, 4> member) cannot be default-constructed
    // and then assigned in the constructor body; it must be initialized
    // directly in the member-initializer list.
    TopNav()
        : tabs_{Button{20.0f, 20.0f, 180.0f, 44.0f},
                Button{210.0f, 20.0f, 180.0f, 44.0f},
                Button{400.0f, 20.0f, 180.0f, 44.0f},
                Button{590.0f, 20.0f, 180.0f, 44.0f},
                Button{780.0f, 20.0f, 180.0f, 44.0f}} {}

    // Recomputes tab rects from the current window size — call once per
    // frame before update()/draw() so resizing never leaves stale rects.
    //
    // Mutates tabs_[i]'s x/y/w/h fields in place rather than reassigning a
    // freshly-constructed `Button{...}`: Button's press-tracking state
    // (wasDownLastFrame_, private) has no default-preserving copy path
    // through a temporary built from the 4-arg constructor (it always
    // starts false), so `tabs_[i] = Button{r.x,r.y,r.w,r.h};` here would
    // silently wipe the "was pressed last frame" flag on every single
    // frame — since updateLayout() runs before update() every frame, this
    // makes a completed down-then-up click on any tab structurally
    // undetectable (verified interactively: real clicks never registered
    // until this was changed to field mutation).
    void updateLayout(Rect windowRect, float uiScaleFactor) {
        // UiScale is purely height-derived and uncapped above (see
        // ui/layout.h), so a tall window can scale the tab strip's natural
        // width past the window's actual width, pushing the last tab(s) off
        // the right edge entirely (observed: the "Math" tab disappearing
        // when maximized). Clamp the scale used for the nav strip's own
        // sizing to whatever additionally fits the real window width, so
        // tabs shrink gracefully together instead of clipping off-screen.
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
        contentArea_ = windowRect; // what dockTop left behind, for pages to use
    }

    const Rect& contentArea() const { return contentArea_; }

    Page update(const InputState& input, Page current) {
        for (int i = 0; i < 5; i++) {
            if (tabs_[i].update(input)) return (Page)i;
        }
        return current;
    }

    bool hoversAnyTab(const InputState& input) const {
        for (int i = 0; i < 5; i++) {
            if (tabs_[i].hovered(input)) return true;
        }
        return false;
    }

    void draw(PrimitiveBatch& batch, TextRenderer& text, Page current) const {
        const char* labels[5] = {"Text", "Shapes & Curves", "Widgets", "Animation", "Math"};
        // Label size follows the same clamped navScale_ as the tab rects —
        // a fixed 18px label overflows (and visually swallows) a tab that
        // the width clamp shrank below its natural size.
        float labelSize = 18.0f * navScale_;
        for (int i = 0; i < 5; i++) {
            float gray = ((Page)i == current) ? 0.8f : 0.4f;
            batch.pushRoundedRect(tabs_[i].x, tabs_[i].y, tabs_[i].w, tabs_[i].h, 6.0f, gray);
            float textW = text.textWidth(labels[i], labelSize);
            text.drawText(labels[i], tabs_[i].x + (tabs_[i].w - textW) * 0.5f,
                          tabs_[i].y + tabs_[i].h * 0.5f + 6.0f * navScale_, labelSize, 0.05f);
        }
    }

 private:
    std::array<Button, 5> tabs_;
    float navScale_ = 1.0f;
    Rect contentArea_{0, 0, 0, 0};
};
