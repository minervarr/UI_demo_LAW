// Concrete mathcore::IMathFontMetrics adapter over the vendored Vulkan font
// engine's real MsdfFont (libs/firstparty/vulkan_font_engine/core/msdf.hh).
// mathcore (libs/mathcore) has zero knowledge of MsdfFont/Vulkan — it only
// knows the POD mirror types in <mathcore/imath_font_metrics.h>. Every method
// here is a thin forwarding call that converts the real MsdfFont/
// MathConstants/MsdfGlyph/MathConstruction/VStretch types into those mirrors.
// POD-to-POD copies only, cheap, not perf-sensitive (called once per layout
// pass, not per frame-critical hot loop beyond that).
#pragma once
#include <cstdint>
#include <unordered_map>

#include <mathcore/imath_font_metrics.h>

#include "msdf.hh"  // real MsdfFont, from libs/firstparty/vulkan_font_engine/core

class MsdfFontMetricsAdapter : public mathcore::IMathFontMetrics {
 public:
    explicit MsdfFontMetricsAdapter(const MsdfFont& font) : font_(font) {}

    bool hasMath() const override { return font_.hasMath(); }
    const mathcore::MathConstants& mathConstants() const override;
    float glyphPadEm() const override { return font_.glyphPadEm(); }
    uint32_t mathKey(uint32_t codepoint) const override { return font_.mathKey(codepoint); }
    uint32_t keyForStyle(mathcore::FontStyle style, uint32_t codepoint) const override {
        return font_.keyForStyle(toRealStyle(style), codepoint);
    }
    const mathcore::GlyphMetrics* glyphByKey(uint32_t key) const override;
    const mathcore::MathConstruction* construction(uint32_t key) const override;
    mathcore::VStretch buildVStretch(const mathcore::MathConstruction& c,
                                     float targetEm) const override;
    float advanceKey(uint32_t key, float sizePx) const override {
        return font_.advanceKey(key, sizePx);
    }

 private:
    static FontStyle toRealStyle(mathcore::FontStyle style);

    // Opaque bridge: mathcore::MathConstruction carries no fields of its own
    // (see imath_font_metrics.h's comment), so this adapter subclasses it to
    // smuggle the real MathConstruction* back through construction()'s
    // mathcore::MathConstruction* return type, recovered in buildVStretch()
    // via a static_cast (safe: every instance handed out by construction()
    // below is actually one of these).
    struct AdapterConstruction : mathcore::MathConstruction {
        const ::MathConstruction* real = nullptr;
    };

    const MsdfFont& font_;
    mutable mathcore::MathConstants constantsCache_{};
    mutable bool constantsCacheValid_ = false;
    mutable std::unordered_map<uint32_t, mathcore::GlyphMetrics> glyphCache_;
    mutable std::unordered_map<uint32_t, AdapterConstruction> constructionCache_;
};
