#pragma once

// New in mathcore (not ported from calculator): mathcore's own color type.
// windows_ui_demo is grayscale-only (every fragment shader writes R==G==B),
// but mathcore itself is a general-purpose library and must not bake that
// constraint into its public API — so its draw calls take a full Ink{r,g,b,a}
// rather than a single `float gray`. windows_ui_demo call sites use the
// `Ink::gray(v)` factory, a mechanical 1:1 substitution for the old
// `float gray` parameter.

namespace mathcore {

struct Ink {
    float r = 0.9f, g = 0.9f, b = 0.9f, a = 1.0f;  // default matches every
                                                    // current 0.9f gray default
    static constexpr Ink gray(float v, float alpha = 1.0f) { return {v, v, v, alpha}; }
};

}  // namespace mathcore
