#pragma once
#include "../gfx/primitives.h"
#include "../platform/input_state.h"
#include "../text/text_renderer.h"
#include "../ui/layout.h"

// Multi-script showcase strings. Declared here (not just inline in
// page_text.cpp) so main.cpp can pass the exact same strings to
// TextRenderer::bakeFonts()'s extraTexts, keeping "what gets baked" and
// "what gets drawn" as one source of truth.
extern const char* kChineseSample;
extern const char* kJapaneseSample;
extern const char* kKoreanSample;

class TextPage {
 public:
    // Applies this frame's mouse-wheel movement to the page's scroll offset,
    // clamped against the content height measured by the previous draw()
    // (one frame stale on the first frame/after a resize — harmless).
    void update(const InputState& input, Rect area);
    void draw(PrimitiveBatch& batch, TextRenderer& text, Rect area, float uiScaleFactor);

 private:
    float scrollY_ = 0.0f;
    float contentHeight_ = 0.0f;
};
