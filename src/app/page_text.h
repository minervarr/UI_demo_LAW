#pragma once
#include "canvas.hh"
#include "frame_input.hh"

// Multi-script showcase strings. Declared here (not just inline in
// page_text.cpp) so main.cpp can bake the exact same strings' codepoints via
// the CJK fallback fonts, keeping "what gets baked" and "what gets drawn" as
// one source of truth.
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
