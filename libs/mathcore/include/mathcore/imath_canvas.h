#pragma once
#include <cstdint>
#include <string_view>

#include <mathcore/ink.h>
#include <mathcore/imath_font_metrics.h>

// New in mathcore (not ported from calculator): the drawing-surface half of
// the rendering-backend seam (see imath_font_metrics.h for the font-metrics
// half). mathbox.h/math_layout.h/the editor/graphing code depend only on
// this interface and IMathFontMetrics — zero knowledge of MsdfFont, the
// Vulkan font engine, or Vulkan itself anywhere in mathcore. windows_ui_demo's
// MathCanvas becomes a concrete IMathCanvas implementation (added in a later
// task) over its existing PrimitiveBatch/TextRenderer.

namespace mathcore {

class IMathCanvas {
public:
    virtual ~IMathCanvas() = default;
    virtual const IMathFontMetrics* mathFontMetrics() const = 0;   // null when no math font loaded
    virtual void  text(std::string_view s, float x, float y, float size, Ink ink) = 0;
    virtual float textWidth(std::string_view s, float size) const = 0;
    virtual void  rect(float x, float y, float w, float h, Ink ink) = 0;
    virtual void  mathGlyph(uint32_t key, float x, float y, float size, Ink ink) = 0;
};

}  // namespace mathcore
