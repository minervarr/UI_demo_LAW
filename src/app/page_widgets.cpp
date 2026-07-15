#include "page_widgets.h"

#include <cstdio>

#include "layout.hh"
#include "gray.h"

WidgetsPage::WidgetsPage() {
    demoList_.itemCount = (int)listLabels_.size();
}

void WidgetsPage::updateLayout(Rect area, float uiScaleFactor) {
    // Widgets' x/y/w/h are mutated in place, never reassigned via a fresh
    // temporary — see widgets_gray.h's class comment for why (private
    // click-tracking state would silently reset every frame).
    ColumnCursor col(area.x + 60.0f * uiScaleFactor, area.y + 60.0f * uiScaleFactor,
                     20.0f * uiScaleFactor);
    Rect r = col.next(200.0f * uiScaleFactor, 50.0f * uiScaleFactor);
    demoButton_.x = r.x; demoButton_.y = r.y; demoButton_.w = r.w; demoButton_.h = r.h;
    r = col.next(60.0f * uiScaleFactor, 32.0f * uiScaleFactor);
    demoToggle_.x = r.x; demoToggle_.y = r.y; demoToggle_.w = r.w; demoToggle_.h = r.h;
    r = col.next(300.0f * uiScaleFactor, 20.0f * uiScaleFactor);
    demoSlider_.x = r.x; demoSlider_.y = r.y; demoSlider_.w = r.w; demoSlider_.h = r.h;
    r = col.next(260.0f * uiScaleFactor, 30.0f * uiScaleFactor * demoList_.itemCount);
    demoList_.x = r.x; demoList_.y = r.y; demoList_.w = r.w; demoList_.h = r.h;
    demoList_.rowHeight = 30.0f * uiScaleFactor;
}

void WidgetsPage::update(const FrameInput& input) {
    if (demoButton_.update(input)) clickCount_++;
    demoToggle_.update(input);
    demoSlider_.update(input);
    demoList_.update(input);
}

void WidgetsPage::draw(Canvas& canvas) {
    // Side labels take a text-box TOP: the old renderer's baseline
    // (centerY + 7) minus the 18px size.
    demoButton_.draw(canvas, "Click me");
    char clickLabel[32];
    std::snprintf(clickLabel, sizeof(clickLabel), "Clicks: %d", clickCount_);
    canvas.text(clickLabel, demoButton_.x + demoButton_.w + 20.0f,
                demoButton_.y + demoButton_.h * 0.5f + 7.0f - 18.0f, 18.0f, gray(0.9f));

    demoToggle_.draw(canvas);
    canvas.text(demoToggle_.on ? "On" : "Off", demoToggle_.x + demoToggle_.w + 20.0f,
                demoToggle_.y + demoToggle_.h * 0.5f + 7.0f - 18.0f, 18.0f, gray(0.9f));

    demoSlider_.draw(canvas);
    char sliderLabel[32];
    std::snprintf(sliderLabel, sizeof(sliderLabel), "%.0f", demoSlider_.value);
    canvas.text(sliderLabel, demoSlider_.x + demoSlider_.w + 20.0f,
                demoSlider_.y + demoSlider_.h * 0.5f + 7.0f - 18.0f, 18.0f, gray(0.9f));

    demoList_.draw(canvas, listLabels_);
}
