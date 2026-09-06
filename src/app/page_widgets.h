#pragma once
#include <string>
#include <vector>

#include "canvas.hh"
#include "frame_input.hh"
#include "widgets_gray.h"

class WidgetsPage {
 public:
    WidgetsPage();
    void updateLayout(Rect area, float uiScaleFactor);
    void update(const FrameInput& input);
    void draw(Canvas& canvas);
    // Whether the pointer is over any interactive control — the frame loop
    // uses this to switch to the hand cursor.
    bool hoversAnyWidget(const FrameInput& input) const {
        return demoButton_.hovered(input) || demoToggle_.hovered(input) ||
               demoSlider_.hovered(input) || demoList_.hovered(input);
    }
    float contentWidth()  const { return contentW_; }
    float contentHeight() const { return contentH_; }

 private:
    Button demoButton_{60, 140, 200, 50};
    int clickCount_ = 0;
    Toggle demoToggle_{60, 220, 60, 32};
    Slider demoSlider_{60, 300, 300, 20, 0.0f, 100.0f, 40.0f};
    // h must cover the whole list, not just one row: ListBox::update
    // hit-tests against the full rect before resolving the row, so a
    // single-row h makes every row past the first unclickable.
    ListBox demoList_{60, 360, 260, 150, 30.0f};
    std::vector<std::string> listLabels_{"Alpha", "Bravo", "Charlie", "Delta", "Echo"};
    float contentW_ = 0.0f;
    float contentH_ = 0.0f;
};
