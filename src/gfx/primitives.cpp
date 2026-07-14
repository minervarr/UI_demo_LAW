#include "primitives.h"
#include <cmath>

void PrimitiveBatch::setClip(float x, float y, float w, float h) {
    clipActive_ = true;
    clipX_ = x; clipY_ = y; clipW_ = w; clipH_ = h;
}

void PrimitiveBatch::clearClip() {
    clipActive_ = false;
}

const std::vector<DrawRange>& PrimitiveBatch::ranges() const {
    return ranges_;
}

// Opens a new range if the current clip state differs from the open one's
// (or none is open yet); otherwise the open range keeps accumulating.
void PrimitiveBatch::beginRange() {
    if (!ranges_.empty()) {
        const DrawRange& cur = ranges_.back();
        bool sameState = cur.hasClip == clipActive_ &&
            (!clipActive_ || (cur.clipX == clipX_ && cur.clipY == clipY_ &&
                              cur.clipW == clipW_ && cur.clipH == clipH_));
        if (sameState) return;
    }
    DrawRange r;
    r.firstVert = verts_.size();
    r.hasClip = clipActive_;
    r.clipX = clipX_; r.clipY = clipY_; r.clipW = clipW_; r.clipH = clipH_;
    ranges_.push_back(r);
}

void PrimitiveBatch::pushQuad(float ax, float ay, float bx, float by,
                              float cx, float cy, float dx, float dy,
                              float gray, float alpha,
                              float capAx, float capAy, float capBx, float capBy,
                              float capRadius, SdfKind kind) {
    beginRange();
    float k = static_cast<float>(kind);
    // a-b-c-d wound as a quad: triangles (a,b,c) and (a,c,d)
    verts_.push_back({ax, ay, gray, alpha, capAx, capAy, capBx, capBy, capRadius, k});
    verts_.push_back({bx, by, gray, alpha, capAx, capAy, capBx, capBy, capRadius, k});
    verts_.push_back({cx, cy, gray, alpha, capAx, capAy, capBx, capBy, capRadius, k});
    verts_.push_back({ax, ay, gray, alpha, capAx, capAy, capBx, capBy, capRadius, k});
    verts_.push_back({cx, cy, gray, alpha, capAx, capAy, capBx, capBy, capRadius, k});
    verts_.push_back({dx, dy, gray, alpha, capAx, capAy, capBx, capBy, capRadius, k});
    ranges_.back().vertCount = verts_.size() - ranges_.back().firstVert;
}

void PrimitiveBatch::pushRect(float x, float y, float w, float h, float gray, float alpha) {
    pushQuad(x, y, x + w, y, x + w, y + h, x, y + h, gray, alpha);
}

void PrimitiveBatch::pushLine(float x0, float y0, float x1, float y1, float thickness,
                              float gray, float alpha) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) return;
    float radius = thickness * 0.5f;
    // Expand the quad beyond the true capsule (segment + radius) by a small
    // antialiasing margin, on every side: perpendicular to the segment (so
    // there's room for the SDF to soften the side edges) and along the
    // segment direction past both endpoints (so there's room for the round
    // caps). The fragment shader's capsule SDF (using the real x0,y0/x1,y1/
    // radius carried per-vertex) then draws the actual antialiased shape
    // inside this generous quad; pixels between the true capsule edge and
    // the quad's outer edge get zero coverage and are fully transparent.
    const float margin = 1.5f;
    float extend = radius + margin;
    float dirX = dx / len, dirY = dy / len;
    float perpX = -dirY * extend, perpY = dirX * extend;
    float extX = dirX * extend, extY = dirY * extend;

    float ax = x0 - extX + perpX, ay = y0 - extY + perpY;
    float bx = x1 + extX + perpX, by = y1 + extY + perpY;
    float cx = x1 + extX - perpX, cy = y1 + extY - perpY;
    float dx2 = x0 - extX - perpX, dy2 = y0 - extY - perpY;
    pushQuad(ax, ay, bx, by, cx, cy, dx2, dy2, gray, alpha,
             x0, y0, x1, y1, radius, SdfKind::Capsule);
}

void PrimitiveBatch::pushBezier(float x0, float y0, float cx0, float cy0,
                                float cx1, float cy1, float x1, float y1,
                                float thickness, float gray, float alpha, int segments) {
    if (segments <= 0) {
        // Adaptive count from the control polygon's length — an upper bound
        // on the curve's true arc length, so ~1 segment per 1.75 screen px
        // keeps each straight piece at sub-facet scale at any zoom (the same
        // pixel-density rule the sibling calculator app's plotter uses).
        // Clamped: >=8 so tiny curves stay round, <=256 caps the vertex cost
        // (256 segments * 6 verts = 1536, far under the 65536 batch limit).
        float len = std::sqrt((cx0 - x0) * (cx0 - x0) + (cy0 - y0) * (cy0 - y0)) +
                    std::sqrt((cx1 - cx0) * (cx1 - cx0) + (cy1 - cy0) * (cy1 - cy0)) +
                    std::sqrt((x1 - cx1) * (x1 - cx1) + (y1 - cy1) * (y1 - cy1));
        segments = (int)std::ceil(len / 1.75f);
        if (segments < 8) segments = 8;
        if (segments > 256) segments = 256;
    }
    float prevX = x0, prevY = y0;
    for (int i = 1; i <= segments; i++) {
        float t = (float)i / (float)segments;
        float u = 1.0f - t;
        float x = u * u * u * x0 + 3 * u * u * t * cx0 + 3 * u * t * t * cx1 + t * t * t * x1;
        float y = u * u * u * y0 + 3 * u * u * t * cy0 + 3 * u * t * t * cy1 + t * t * t * y1;
        pushLine(prevX, prevY, x, y, thickness, gray, alpha);
        prevX = x; prevY = y;
    }
}

void PrimitiveBatch::pushRoundedRect(float x, float y, float w, float h, float radius,
                                     float gray, float alpha, int /*cornerSegments, unused*/) {
    if (radius <= 0.0f) { pushRect(x, y, w, h, gray, alpha); return; }
    // Clamp so the "core" half-extents (below) never go negative — a radius
    // requested larger than half the shorter side degrades gracefully to a
    // circle/stadium instead of producing an inverted/garbage SDF.
    float r = std::min(radius, std::min(w, h) * 0.5f);

    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float halfW = w * 0.5f, halfH = h * 0.5f;
    // Core half-extents: the box shrunk by the corner radius on each axis.
    // When one axis is 0 this is a "stadium"/pill (e.g. a toggle track);
    // when both are 0 it's a plain circle (e.g. a toggle/slider knob) — the
    // same rounded-box SDF formula (see primitives.h's Vertex comment)
    // handles all three cases with no special-casing needed here.
    float coreHalfW = halfW - r, coreHalfH = halfH - r;

    // One antialiased quad for the whole shape (edges AND corners), matching
    // pushLine's technique: expand outward by a small AA margin so the
    // shader's fwidth-based coverage has room to soften the true edge.
    const float margin = 1.5f;
    float ex = halfW + margin, ey = halfH + margin;
    pushQuad(cx - ex, cy - ey, cx + ex, cy - ey, cx + ex, cy + ey, cx - ex, cy + ey,
             gray, alpha, cx, cy, coreHalfW, coreHalfH, r, SdfKind::Box);
}
