#include "msdf_font_metrics_adapter.h"

FontStyle MsdfFontMetricsAdapter::toRealStyle(mathcore::FontStyle style) {
    switch (style) {
        case mathcore::FontStyle::Bold:   return FontStyle::Bold;
        case mathcore::FontStyle::Italic: return FontStyle::Italic;
        case mathcore::FontStyle::Roman:
        default:                          return FontStyle::Roman;
    }
}

const mathcore::MathConstants& MsdfFontMetricsAdapter::mathConstants() const {
    if (!constantsCacheValid_) {
        const MathConstants& mc = font_.mathConstants();
        mathcore::MathConstants out;
        out.scriptPercentScaleDown = mc.scriptPercentScaleDown();
        out.scriptScriptPercentScaleDown = mc.scriptScriptPercentScaleDown();
        out.axisHeight = mc.axisHeight();
        out.subscriptShiftDown = mc.subscriptShiftDown();
        out.subscriptTopMax = mc.subscriptTopMax();
        out.superscriptShiftUp = mc.superscriptShiftUp();
        out.superscriptBottomMin = mc.superscriptBottomMin();
        out.spaceAfterScript = mc.spaceAfterScript();
        out.fractionNumeratorShiftUp = mc.fractionNumeratorShiftUp();
        out.fractionNumeratorDisplayStyleShiftUp = mc.fractionNumeratorDisplayStyleShiftUp();
        out.fractionDenominatorShiftDown = mc.fractionDenominatorShiftDown();
        out.fractionDenominatorDisplayStyleShiftDown = mc.fractionDenominatorDisplayStyleShiftDown();
        out.fractionNumeratorGapMin = mc.fractionNumeratorGapMin();
        out.fractionNumDisplayStyleGapMin = mc.fractionNumDisplayStyleGapMin();
        out.fractionRuleThickness = mc.fractionRuleThickness();
        out.fractionDenominatorGapMin = mc.fractionDenominatorGapMin();
        out.fractionDenomDisplayStyleGapMin = mc.fractionDenomDisplayStyleGapMin();
        out.radicalVerticalGap = mc.radicalVerticalGap();
        out.radicalDisplayStyleVerticalGap = mc.radicalDisplayStyleVerticalGap();
        out.radicalRuleThickness = mc.radicalRuleThickness();
        out.radicalExtraAscender = mc.radicalExtraAscender();
        out.radicalKernBeforeDegree = mc.radicalKernBeforeDegree();
        out.radicalKernAfterDegree = mc.radicalKernAfterDegree();
        out.radicalDegreeBottomRaisePercent = mc.radicalDegreeBottomRaisePercent();
        constantsCache_ = out;
        constantsCacheValid_ = true;
    }
    return constantsCache_;
}

const mathcore::GlyphMetrics* MsdfFontMetricsAdapter::glyphByKey(uint32_t key) const {
    auto it = glyphCache_.find(key);
    if (it != glyphCache_.end()) return &it->second;

    const MsdfGlyph* g = font_.glyphByKey(key);
    if (!g) return nullptr;

    mathcore::GlyphMetrics gm;
    gm.planeT = g->planeT;
    gm.planeB = g->planeB;
    gm.planeL = g->planeL;
    gm.planeR = g->planeR;
    gm.hasGlyph = g->hasGlyph;
    gm.advance = g->advance;
    gm.italic = g->italic;
    auto [ins, ok] = glyphCache_.emplace(key, gm);
    return &ins->second;
}

const mathcore::MathConstruction* MsdfFontMetricsAdapter::construction(uint32_t key) const {
    auto it = constructionCache_.find(key);
    if (it != constructionCache_.end()) return &it->second;

    const ::MathConstruction* real = font_.construction(key);
    if (!real) return nullptr;

    AdapterConstruction ac;
    ac.real = real;
    auto [ins, ok] = constructionCache_.emplace(key, ac);
    return &ins->second;
}

mathcore::VStretch MsdfFontMetricsAdapter::buildVStretch(const mathcore::MathConstruction& c,
                                                         float targetEm) const {
    const auto& ac = static_cast<const AdapterConstruction&>(c);
    VStretch real = font_.buildVStretch(*ac.real, targetEm);

    mathcore::VStretch out;
    out.single = real.single;
    out.key = real.key;
    out.heightEm = real.heightEm;
    out.parts.reserve(real.parts.size());
    for (const PlacedPart& pp : real.parts) {
        out.parts.push_back({pp.key, pp.bottomEm});
    }
    return out;
}
