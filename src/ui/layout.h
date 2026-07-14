#pragma once
#include <algorithm>

struct Rect { float x, y, w, h; };

// Same scaling formula as windows_matrix_player's ResponsiveTextScale
// (libs/firstparty/vk_canvas/core/responsive_text.hh): a value declared at
// `referenceHeight` scales linearly with the actual window height, floored
// (never capped) so controls shrink gracefully on a small window but keep
// growing on a large one instead of staying pinned to their reference size.
struct UiScale {
    float referenceHeight;
    float floorScale = 0.5f;

    float factor(float actualHeight) const {
        return std::max(actualHeight / referenceHeight, floorScale);
    }
    // factor() with a ceiling on top of the floor. Pages whose content
    // stacks vertically use this: uncapped scaling grows the stack faster
    // than the window (padding/gaps scale too), pushing the last rows below
    // the framebuffer on a maximized window.
    float cappedFactor(float actualHeight, float cap) const {
        return std::min(factor(actualHeight), cap);
    }
    float scale(float value, float actualHeight) const {
        return value * factor(actualHeight);
    }
};

// Clamps a scroll offset so the view never scrolls above the content top
// (< 0) or past its bottom; content shorter than the view can't scroll.
inline float clampScroll(float scrollY, float contentH, float viewH) {
    return std::max(0.0f, std::min(scrollY, std::max(0.0f, contentH - viewH)));
}

Rect dockTop(Rect& container, float thickness);
Rect dockBottom(Rect& container, float thickness);
Rect dockLeft(Rect& container, float thickness);
Rect dockRight(Rect& container, float thickness);

Rect centerIn(const Rect& container, float w, float h);

class RowCursor {
 public:
    RowCursor(float startX, float y, float gap) : x_(startX), y_(y), gap_(gap) {}
    Rect next(float w, float h) {
        Rect r{x_, y_, w, h};
        x_ += w + gap_;
        return r;
    }
 private:
    float x_, y_, gap_;
};

class ColumnCursor {
 public:
    ColumnCursor(float x, float startY, float gap) : x_(x), y_(startY), gap_(gap) {}
    Rect next(float w, float h) {
        Rect r{x_, y_, w, h};
        y_ += h + gap_;
        return r;
    }
 private:
    float x_, y_, gap_;
};
