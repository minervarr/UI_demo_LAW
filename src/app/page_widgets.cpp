#include "page_widgets.h"
#include <cstdio>

WidgetsPage::WidgetsPage() {
    demoList_.itemCount = (int)listLabels_.size();
}

void WidgetsPage::updateLayout(Rect area, float uiScaleFactor) {
    // Every widget's x/y/w/h is mutated in place, never reassigned via a
    // freshly-constructed `Button{...}`/`Toggle{...}` temporary: those
    // types track click/drag state in private fields (Button's
    // wasDownLastFrame_, Toggle's own wasDownLastFrame_) that a temporary
    // built from the position-only constructor always initializes to
    // false. Since updateLayout() runs every frame before update(), a
    // full-struct reassignment here would silently erase "was pressed
    // last frame" every frame and make a completed down-then-up click
    // structurally undetectable (found interactively: real clicks on
    // demoButton_/demoToggle_ never registered until this was changed
    // to field mutation — same root cause as the nav tabs, see nav.h).
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

void WidgetsPage::update(const InputState& input) {
    if (demoButton_.update(input)) clickCount_++;
    demoToggle_.update(input);
    demoSlider_.update(input);
    demoList_.update(input);
}

void WidgetsPage::draw(PrimitiveBatch& batch, TextRenderer& text) {
    demoButton_.draw(batch, text, "Click me");
    char clickLabel[32];
    std::snprintf(clickLabel, sizeof(clickLabel), "Clicks: %d", clickCount_);
    text.drawText(clickLabel, demoButton_.x + demoButton_.w + 20.0f, demoButton_.y + demoButton_.h * 0.5f + 7.0f, 18.0f, 0.9f);

    demoToggle_.draw(batch);
    text.drawText(demoToggle_.on ? "On" : "Off", demoToggle_.x + demoToggle_.w + 20.0f, demoToggle_.y + demoToggle_.h * 0.5f + 7.0f, 18.0f, 0.9f);

    demoSlider_.draw(batch);
    char sliderLabel[32];
    std::snprintf(sliderLabel, sizeof(sliderLabel), "%.0f", demoSlider_.value);
    text.drawText(sliderLabel, demoSlider_.x + demoSlider_.w + 20.0f, demoSlider_.y + demoSlider_.h * 0.5f + 7.0f, 18.0f, 0.9f);

    demoList_.draw(batch, text, listLabels_);
}
