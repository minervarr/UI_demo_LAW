#pragma once
#include "canvas.hh"
#include "frame_input.hh"

// Multi-script showcase strings.
//
// They used to be declared here so the app could bake their exact codepoints
// into the atlas at startup. Nothing bakes them now: the UI font registers
// per-style fallback chains and rasterizes what it is asked for, so these are
// just strings and the "keep the bake list in step with the draw list" problem
// they existed to solve is gone.
extern const char* kChineseSample;
extern const char* kJapaneseSample;
extern const char* kKoreanSample;

class TextPage {
 public:
    // Scrolling is no longer this page's business. It used to own a wheel
    // offset, which meant it could not be reached at all on a touch screen;
    // the frame loop now hands it an already-shifted rect and reads back the
    // height it produced. See scroll_area.h.
    void draw(Canvas& canvas, Rect area, float uiScaleFactor);

    float contentWidth()  const { return contentWidth_; }
    float contentHeight() const { return contentHeight_; }

 private:
    float contentWidth_  = 0.0f;
    float contentHeight_ = 0.0f;
};
