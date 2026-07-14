#version 450
layout(location = 0) in float inGray;
layout(location = 1) in float inAlpha;
layout(location = 2) in vec2 inFragPos;
layout(location = 3) in vec2 inCapA;
layout(location = 4) in vec2 inCapB;
layout(location = 5) in float inRadius;
layout(location = 6) in float inKind;
layout(location = 0) out vec4 outColor;

void main() {
    // Grayscale-by-construction: R, G, B are always the same value; only
    // alpha (coverage) varies below.
    if (inKind < 0.5) {
        // Flat shape (plain rect): no SDF, hard edges.
        outColor = vec4(inGray, inGray, inGray, inAlpha);
        return;
    }

    float d;
    if (inKind < 1.5) {
        // Analytic capsule SDF: distance from this fragment to the line
        // segment (inCapA -> inCapB), offset by the capsule radius. Used
        // for lines and bezier segments (round-capped capsules).
        vec2 pa = inFragPos - inCapA;
        vec2 ba = inCapB - inCapA;
        float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
        d = length(pa - ba * h) - inRadius;
    } else {
        // Analytic rounded-box SDF (Inigo Quilez's sdRoundBox): inCapA is
        // the box center, inCapB is the box's CORE half-extent (half-width/
        // half-height each already shrunk by the corner radius), inRadius
        // is the corner radius. Degenerates to a stadium/pill when one core
        // half-extent is 0, or a plain circle when both are 0 — used for
        // rounded rects, and (via zero/one-axis-zero core extents) circular
        // and pill-shaped widget knobs/tracks.
        vec2 q = abs(inFragPos - inCapA) - inCapB;
        d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - inRadius;
    }
    // Converted to a screen-pixel-wide antialiased coverage value via
    // fwidth(), so every SDF shape gets soft, resolution-independent edges
    // without MSAA.
    float w = max(fwidth(d), 1e-6);
    float cov = clamp(0.5 - d / w, 0.0, 1.0);
    outColor = vec4(inGray, inGray, inGray, inAlpha * cov);
}
