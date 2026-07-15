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
    // Applies this frame's wheel movement to the scroll offset, clamped
    // against the content height measured by the previous draw() (one frame
    // stale on the first frame/after a resize — harmless).
    void update(const FrameInput& input, Rect area);
    void draw(Canvas& canvas, Rect area, float uiScaleFactor);

 private:
    float scrollY_ = 0.0f;
    float contentHeight_ = 0.0f;
};
