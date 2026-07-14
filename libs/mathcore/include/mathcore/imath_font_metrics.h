#pragma once
#include <cstdint>
#include <vector>

// New in mathcore (not ported from calculator): the rendering-backend seam
// that decouples mathbox/math_layout (and everything built on them) from the
// vendored Vulkan font engine's `MsdfFont`. `mathbox.cpp` calls a real, wide
// surface of `MsdfFont` methods (hasMath, mathConstants, glyphByKey,
// glyphPadEm, mathKey, construction, buildVStretch, advanceKey, keyForStyle)
// plus several vendored value types (MathConstants, MsdfGlyph,
// MathConstruction, VStretch) — see libs/firstparty/vulkan_font_engine/core/
// msdf.hh. `IMathFontMetrics` mirrors exactly that surface (same method
// names/signatures) so mathcore has zero knowledge of MsdfFont/Vulkan; a
// concrete adapter (windows_ui_demo's MsdfFontMetricsAdapter, added in a
// later task) wraps the real MsdfFont and converts its real types into the
// POD mirrors below.
//
// Every field below was enumerated from the real `mf->...`/`c.msdfFont()->...`
// call sites in the *current* src/math/mathbox.cpp (not yet moved into this
// library) and cross-referenced against msdf.hh's real MathConstants/
// MsdfGlyph/MathConstruction/VStretch/MsdfFont definitions — see
// .superpowers/sdd/task-2-report.md for the full call-site-by-call-site
// citation. Nothing here was guessed: only accessors mathbox.cpp actually
// calls are present, and each keeps the exact real name.

namespace mathcore {

enum class FontStyle : uint8_t { Roman, Bold, Italic };

// Mirrors msdf.hh's `MathConstants` (baked OpenType MATH table constants).
// The real type exposes 42 baked values via named `float xxx() const`
// accessors; only the 24 mathbox.cpp actually reads are mirrored here, as
// plain public fields (all in em units, matching the real accessors'
// return values 1:1 — see task-2-report.md for the mf->mathConstants().xxx()
// call-site citations backing every one of these).
struct MathConstants {
    // scaling
    float scriptPercentScaleDown = 0;
    float scriptScriptPercentScaleDown = 0;
    float axisHeight = 0;
    // scripts
    float subscriptShiftDown = 0;
    float subscriptTopMax = 0;
    float superscriptShiftUp = 0;
    float superscriptBottomMin = 0;
    float spaceAfterScript = 0;
    // fractions
    float fractionNumeratorShiftUp = 0;
    float fractionNumeratorDisplayStyleShiftUp = 0;
    float fractionDenominatorShiftDown = 0;
    float fractionDenominatorDisplayStyleShiftDown = 0;
    float fractionNumeratorGapMin = 0;
    float fractionNumDisplayStyleGapMin = 0;
    float fractionRuleThickness = 0;
    float fractionDenominatorGapMin = 0;
    float fractionDenomDisplayStyleGapMin = 0;
    // radicals
    float radicalVerticalGap = 0;
    float radicalDisplayStyleVerticalGap = 0;
    float radicalRuleThickness = 0;
    float radicalExtraAscender = 0;
    float radicalKernBeforeDegree = 0;
    float radicalKernAfterDegree = 0;
    float radicalDegreeBottomRaisePercent = 0;
};

// Mirrors msdf.hh's `MsdfGlyph`. Only the fields mathbox.cpp actually reads:
// the plane box (planeT/planeB read; planeL/planeR carried for symmetry with
// the plane box concept and future callers, per the plan's given shape),
// hasGlyph (null-vs-blank-glyph distinction), advance, and italic (math
// italic correction).
struct GlyphMetrics {
    float planeT = 0, planeB = 0, planeL = 0, planeR = 0;
    bool  hasGlyph = false;
    float advance = 0;
    float italic = 0;
};

// Mirrors msdf.hh's `MathConstruction`. mathbox.cpp never reads a field of
// this directly — it only ever holds a `const MathConstruction*` obtained
// from `construction()` and passes it straight through to `buildVStretch()`.
// Left opaque-ish on purpose; a concrete adapter is free to carry whatever
// data it needs to bridge back to the real MsdfFont's construction table.
struct MathConstruction {};

// Mirrors msdf.hh's `PlacedPart` — but only the two fields VStretch's
// consumers (radicalDraw/delimDraw in mathbox.cpp) read: `key` (glyph to
// draw) and `bottomEm` (its vertical placement). The real PlacedPart also
// carries `fullEm`, which mathbox.cpp never reads.
struct PlacedPart { uint32_t key; float bottomEm; };

// Mirrors msdf.hh's `VStretch`.
struct VStretch {
    bool     single = false;
    uint32_t key = 0;
    float    heightEm = 0;
    std::vector<PlacedPart> parts;
};

// Mirrors the exact `MsdfFont` method surface mathbox.cpp calls (hasMath,
// mathConstants, glyphByKey, glyphPadEm, mathKey, construction, buildVStretch,
// advanceKey, keyForStyle) — 9 methods, none invented, none omitted.
class IMathFontMetrics {
public:
    virtual ~IMathFontMetrics() = default;
    virtual bool hasMath() const = 0;
    virtual const MathConstants& mathConstants() const = 0;
    virtual float glyphPadEm() const = 0;
    virtual uint32_t mathKey(uint32_t codepoint) const = 0;
    virtual uint32_t keyForStyle(FontStyle, uint32_t codepoint) const = 0;
    virtual const GlyphMetrics* glyphByKey(uint32_t key) const = 0;         // nullable
    virtual const MathConstruction* construction(uint32_t key) const = 0;  // nullable
    virtual VStretch buildVStretch(const MathConstruction&, float targetEm) const = 0;
    virtual float advanceKey(uint32_t key, float sizePx) const = 0;
};

}  // namespace mathcore
