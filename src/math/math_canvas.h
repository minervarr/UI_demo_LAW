#pragma once
#include "../text/text_renderer.h"
#include "../gfx/primitives.h"
#include "msdf_font_metrics_adapter.h"
#include <mathcore/ink.h>
#include <mathcore/imath_canvas.h>
#include <string_view>

// Concrete mathcore::IMathCanvas implementation routed to this app's existing
// PrimitiveBatch/TextRenderer. Same responsibilities as the pre-mathcore
// version of this class (see git history): grayscale ink is still expressed
// via `Ink::gray()` at every call site (this app has no chroma anywhere), but
// the interface itself now takes a full mathcore::Ink, per IMathCanvas.
class MathCanvas : public mathcore::IMathCanvas {
 public:
    // adapter_ always binds to text_.mathFont() (that reference is valid
    // whether or not the math font finished loading — see TextRenderer's
    // mathFont()); mathFontMetrics() below is what actually gates on
    // hasMathFont(), same as the pre-mathcore msdfFont() accessor did.
    MathCanvas(PrimitiveBatch& batch, TextRenderer& text)
        : batch_(batch), text_(text), adapter_(text_.mathFont()) {}

    const mathcore::IMathFontMetrics* mathFontMetrics() const override {
        return text_.hasMathFont() ? &adapter_ : nullptr;
    }
    void text(std::string_view s, float x, float y, float size, mathcore::Ink ink) override {
        text_.drawText(s, x, y, size, ink.r);
    }
    float textWidth(std::string_view s, float size) const override {
        return text_.textWidth(s, size);
    }
    void rect(float x, float y, float w, float h, mathcore::Ink ink) override {
        batch_.pushRect(x, y, w, h, ink.r);
    }
    void mathGlyph(uint32_t key, float x, float y, float size, mathcore::Ink ink) override {
        text_.drawMathGlyph(key, x, y, size, ink.r);
    }
    // Scissors everything drawn until clearClip() to the given rect, on both
    // underlying sinks — shapes and glyphs clip in lockstep when a page
    // scrolls.
    void setClip(float x, float y, float w, float h) {
        batch_.setClip(x, y, w, h);
        text_.setClip(x, y, w, h);
    }
    void clearClip() {
        batch_.clearClip();
        text_.clearClip();
    }

 private:
    PrimitiveBatch& batch_;
    TextRenderer& text_;
    MsdfFontMetricsAdapter adapter_;
};
