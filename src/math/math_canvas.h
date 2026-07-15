#pragma once
#include <string_view>

#include <mathcore/imath_canvas.h>
#include <mathcore/ink.h>

#include "canvas.hh"
#include "msdf.hh"
#include "msdf_font_metrics_adapter.h"

// Concrete mathcore::IMathCanvas implementation routed to vk_canvas's Canvas.
// mathcore stays renderer-agnostic (full Ink at the interface); this app's
// call sites express grayscale via Ink::gray() (see src/app/gray.h for the
// matching app policy on the Canvas side).
class MathCanvas : public mathcore::IMathCanvas {
 public:
    // The MsdfFont is the same offline-baked atlas Canvas::useMsdf() was
    // given (it carries the OpenType MATH data — see atlas_gen); the adapter
    // exposes its MATH metrics to mathcore's layout engine.
    MathCanvas(Canvas& canvas, const MsdfFont& font)
        : canvas_(canvas), font_(font), adapter_(font) {}

    const mathcore::IMathFontMetrics* mathFontMetrics() const override {
        return font_.hasMath() ? &adapter_ : nullptr;
    }
    // mathcore passes a BASELINE y; Canvas::text takes the text-box top
    // (baseline = top + size), hence the -size conversion.
    void text(std::string_view s, float x, float y, float size, mathcore::Ink ink) override {
        canvas_.text(s, x, y - size, size, toColor(ink));
    }
    float textWidth(std::string_view s, float size) const override {
        return canvas_.textWidth(s, size);
    }
    void rect(float x, float y, float w, float h, mathcore::Ink ink) override {
        canvas_.rect(x, y, w, h, toColor(ink));
    }
    void mathGlyph(uint32_t key, float x, float y, float size, mathcore::Ink ink) override {
        canvas_.mathGlyph(key, x, y, size, toColor(ink));
    }
    // Canvas clips shape quads and MSDF glyph quads alike, so one clip keeps
    // shapes and glyphs in lockstep when the page scrolls.
    void setClip(float x, float y, float w, float h) { canvas_.setClip(x, y, w, h); }
    void clearClip() { canvas_.clearClip(); }

 private:
    static Color toColor(mathcore::Ink ink) { return Color{ink.r, ink.g, ink.b, ink.a}; }

    Canvas& canvas_;
    const MsdfFont& font_;
    MsdfFontMetricsAdapter adapter_;
};
