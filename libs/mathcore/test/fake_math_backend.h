#pragma once
// Throwaway (but kept-around, per the plan's Task 3/4/11) fake implementation
// of mathcore::IMathCanvas/IMathFontMetrics — fixed/dummy metrics, no real
// font, no rendering. Purely so mathbox.cpp (and later math_layout.cpp,
// editor_layout.cpp) can be compiled+linked standalone as unit-testable code,
// without dragging in the Vulkan font engine or any real MSDF font data. Not
// a functional/visual test — just a compile/link smoke-check harness.
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include <mathcore/ink.h>
#include <mathcore/imath_canvas.h>
#include <mathcore/imath_font_metrics.h>

namespace mathcore::test {

// Dummy MathConstants: plausible-looking values (roughly Latin-Modern-shaped)
// so layout code exercises its "real MATH font" branches instead of only ever
// falling back to the legacy no-font heuristics.
inline MathConstants fakeMathConstants() {
    MathConstants mc;
    mc.scriptPercentScaleDown = 0.7f;
    mc.scriptScriptPercentScaleDown = 0.5f;
    mc.axisHeight = 0.25f;
    mc.subscriptShiftDown = 0.25f;
    mc.subscriptTopMax = 0.4f;
    mc.superscriptShiftUp = 0.41f;
    mc.superscriptBottomMin = 0.08f;
    mc.spaceAfterScript = 0.05f;
    mc.fractionNumeratorShiftUp = 0.6f;
    mc.fractionNumeratorDisplayStyleShiftUp = 0.68f;
    mc.fractionDenominatorShiftDown = 0.6f;
    mc.fractionDenominatorDisplayStyleShiftDown = 0.68f;
    mc.fractionNumeratorGapMin = 0.05f;
    mc.fractionNumDisplayStyleGapMin = 0.15f;
    mc.fractionRuleThickness = 0.05f;
    mc.fractionDenominatorGapMin = 0.05f;
    mc.fractionDenomDisplayStyleGapMin = 0.15f;
    mc.radicalVerticalGap = 0.06f;
    mc.radicalDisplayStyleVerticalGap = 0.1f;
    mc.radicalRuleThickness = 0.05f;
    mc.radicalExtraAscender = 0.05f;
    mc.radicalKernBeforeDegree = 0.28f;
    mc.radicalKernAfterDegree = -0.42f;
    mc.radicalDegreeBottomRaisePercent = 0.6f;
    return mc;
}

// Dummy IMathFontMetrics: fixed glyph box for every key, no real construction
// table (buildVStretch just fabricates a plausible single-glyph VStretch).
class FakeMathFontMetrics : public IMathFontMetrics {
public:
    bool hasMath() const override { return true; }
    const MathConstants& mathConstants() const override { return constants_; }
    float glyphPadEm() const override { return 0.02f; }
    uint32_t mathKey(uint32_t codepoint) const override { return codepoint; }
    uint32_t keyForStyle(FontStyle, uint32_t codepoint) const override { return codepoint; }

    const GlyphMetrics* glyphByKey(uint32_t key) const override {
        auto it = glyphs_.find(key);
        if (it != glyphs_.end()) return &it->second;
        GlyphMetrics g;
        g.planeT = -0.62f; g.planeB = 0.02f; g.planeL = 0.0f; g.planeR = 0.5f;
        g.hasGlyph = true; g.advance = 0.55f; g.italic = 0.02f;
        auto [ins, ok] = glyphs_.emplace(key, g);
        return &ins->second;
    }

    const MathConstruction* construction(uint32_t key) const override {
        (void)key;
        return &construction_;
    }

    VStretch buildVStretch(const MathConstruction&, float targetEm) const override {
        VStretch vs;
        vs.single = true;
        vs.key = 0x221A;
        vs.heightEm = std::max(0.6f, targetEm);
        return vs;
    }

    float advanceKey(uint32_t /*key*/, float sizePx) const override { return sizePx * 0.55f; }

private:
    MathConstants constants_ = fakeMathConstants();
    MathConstruction construction_{};
    mutable std::unordered_map<uint32_t, GlyphMetrics> glyphs_;
};

// Dummy IMathCanvas: no real drawing (records nothing), text metrics derived
// from a fixed nominal advance-per-character so textWidth/layout math has
// stable, non-degenerate numbers to work with.
class FakeMathCanvas : public IMathCanvas {
public:
    explicit FakeMathCanvas(bool withFont = true)
        : fontMetrics_(withFont ? &fake_ : nullptr) {}

    const IMathFontMetrics* mathFontMetrics() const override { return fontMetrics_; }
    void text(std::string_view, float, float, float, Ink) override {}
    float textWidth(std::string_view s, float size) const override {
        return static_cast<float>(s.size()) * size * 0.55f;
    }
    void rect(float, float, float, float, Ink) override {}
    void mathGlyph(uint32_t, float, float, float, Ink) override {}

private:
    FakeMathFontMetrics fake_;
    const IMathFontMetrics* fontMetrics_;
};

}  // namespace mathcore::test
