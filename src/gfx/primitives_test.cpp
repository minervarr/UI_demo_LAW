#include "primitives.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool nearlyEqual(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main() {
    PrimitiveBatch batch;

    // pushRect emits exactly 6 verts (2 triangles), all with the given gray/alpha
    batch.pushRect(10, 20, 100, 50, 0.75f, 0.9f);
    assert(batch.vertices().size() == 6);
    for (auto& v : batch.vertices()) {
        assert(nearlyEqual(v.gray, 0.75f));
        assert(nearlyEqual(v.alpha, 0.9f));
        // Flat shape: radius sentinel must be -1 (no capsule SDF applied).
        assert(nearlyEqual(v.radius, -1.0f));
    }
    // Corners present: (10,20) and (110,70) must both appear among the 6 verts
    bool sawTopLeft = false, sawBottomRight = false;
    for (auto& v : batch.vertices()) {
        if (nearlyEqual(v.x, 10) && nearlyEqual(v.y, 20)) sawTopLeft = true;
        if (nearlyEqual(v.x, 110) && nearlyEqual(v.y, 70)) sawBottomRight = true;
    }
    assert(sawTopLeft && sawBottomRight);

    // pushRoundedRect emits one antialiased box-SDF quad (6 verts): kind=Box,
    // radius = corner radius, capA = box center, capB = CORE half-extent
    // (half-size minus the corner radius on each axis).
    batch.reset();
    batch.pushRoundedRect(0, 0, 40, 40, 8.0f, 0.5f, 1.0f);
    assert(batch.vertices().size() == 6);
    for (auto& v : batch.vertices()) {
        assert(nearlyEqual(v.gray, 0.5f));
        assert(nearlyEqual(v.alpha, 1.0f));
        assert(nearlyEqual(v.radius, 8.0f));
        assert((int)v.kind == (int)SdfKind::Box);
        assert(nearlyEqual(v.ax, 20.0f) && nearlyEqual(v.ay, 20.0f));    // center
        assert(nearlyEqual(v.bx, 12.0f) && nearlyEqual(v.by, 12.0f));   // half(40)-r(8)
    }

    // A full-circle case (radius == half of both dimensions, e.g. a widget
    // knob): core half-extent degenerates to (0,0) — same SDF formula draws
    // a plain circle, no special-casing needed in pushRoundedRect itself.
    batch.reset();
    batch.pushRoundedRect(0, 0, 20, 20, 10.0f, 0.5f, 1.0f);
    for (auto& v : batch.vertices()) {
        assert(nearlyEqual(v.bx, 0.0f) && nearlyEqual(v.by, 0.0f));
    }

    // A radius requested larger than half the shorter side is clamped rather
    // than producing a negative (garbage) core half-extent.
    batch.reset();
    batch.pushRoundedRect(0, 0, 30, 10, 100.0f, 0.5f, 1.0f);
    for (auto& v : batch.vertices()) {
        assert(v.bx >= -0.001f && v.by >= -0.001f);
    }

    // pushRect stays the flat, no-SDF fast path (unaffected by the above).
    batch.reset();
    batch.pushRect(0, 0, 40, 40, 0.5f, 1.0f);
    for (auto& v : batch.vertices()) {
        assert((int)v.kind == (int)SdfKind::Flat);
    }

    // reset() clears everything
    batch.reset();
    assert(batch.vertices().empty());

    // pushLine emits an expanded quad (6 verts) regardless of orientation.
    // For a horizontal line (0,0)-(100,0), thickness 4 (radius = 2), the
    // quad is expanded by margin=1.5px both perpendicular to the segment
    // and past each endpoint along the segment direction:
    //   extend = radius + margin = 2 + 1.5 = 3.5
    //   corners: (-3.5, ±3.5) and (103.5, ±3.5)
    batch.pushLine(0, 0, 100, 0, 4.0f, 0.5f);
    assert(batch.vertices().size() == 6);
    bool sawCornerA = false, sawCornerB = false, sawCornerC = false, sawCornerD = false;
    for (auto& v : batch.vertices()) {
        assert(nearlyEqual(v.gray, 0.5f));
        assert(nearlyEqual(v.alpha, 1.0f));
        // Real capsule endpoints/radius carried per-vertex (NOT the
        // expanded quad's corners) — this is what the fragment shader's
        // SDF uses to draw the true, antialiased capsule shape.
        assert(nearlyEqual(v.ax, 0.0f) && nearlyEqual(v.ay, 0.0f));
        assert(nearlyEqual(v.bx, 100.0f) && nearlyEqual(v.by, 0.0f));
        assert(nearlyEqual(v.radius, 2.0f));

        if (nearlyEqual(v.x, -3.5f) && nearlyEqual(v.y, 3.5f)) sawCornerA = true;
        if (nearlyEqual(v.x, 103.5f) && nearlyEqual(v.y, 3.5f)) sawCornerB = true;
        if (nearlyEqual(v.x, 103.5f) && nearlyEqual(v.y, -3.5f)) sawCornerC = true;
        if (nearlyEqual(v.x, -3.5f) && nearlyEqual(v.y, -3.5f)) sawCornerD = true;
    }
    assert(sawCornerA && sawCornerB && sawCornerC && sawCornerD);

    // pushBezier with an explicit N segments emits N * 6 verts (one quad per
    // segment) — an explicit positive count is always honored as-is.
    batch.reset();
    batch.pushBezier(0, 0, 10, -20, 40, -20, 50, 0, 2.0f, 0.3f, 1.0f, /*segments=*/24);
    assert(batch.vertices().size() == 24 * 6);
    // Each segment's vertices carry that segment's own endpoints as its
    // capsule (ax,ay)-(bx,by), not the overall curve's endpoints, and the
    // curve's radius (thickness/2 = 1.0).
    for (auto& v : batch.vertices()) {
        assert(nearlyEqual(v.radius, 1.0f));
    }

    // Adaptive tessellation (segments <= 0, the default): count derives from
    // the control-polygon length L = |p0-c0|+|c0-c1|+|c1-p1| as
    // ceil(L / 1.75), clamped to [8, 256].
    // Here L = 100 + 200 + 100 = 400 -> ceil(400/1.75) = 229 segments.
    batch.reset();
    batch.pushBezier(0, 0, 0, 100, 200, 100, 200, 0, 2.0f, 0.3f, 1.0f);
    assert(batch.vertices().size() == 229 * 6);

    // A tiny curve clamps up to the 8-segment minimum (stays visually round)…
    batch.reset();
    batch.pushBezier(0, 0, 1, 1, 2, 1, 3, 0, 1.0f, 0.3f, 1.0f);
    assert(batch.vertices().size() == 8 * 6);

    // …and a huge one clamps down to the 256-segment maximum (bounded cost).
    batch.reset();
    batch.pushBezier(0, 0, 0, 5000, 10000, 5000, 10000, 0, 2.0f, 0.3f, 1.0f);
    assert(batch.vertices().size() == 256 * 6);

    // Segment count grows with on-screen size: the same curve scaled 2x gets
    // strictly more segments (until the clamp).
    batch.reset();
    batch.pushBezier(0, 0, 0, 50, 100, 50, 100, 0, 2.0f, 0.3f, 1.0f);
    size_t smallVerts = batch.vertices().size();
    batch.reset();
    batch.pushBezier(0, 0, 0, 100, 200, 100, 200, 0, 2.0f, 0.3f, 1.0f);
    assert(batch.vertices().size() > smallVerts);

    // --- Clip ranges ---

    // A batch with no clip yields exactly one full-framebuffer range that
    // tiles all vertices.
    batch.reset();
    batch.pushRect(0, 0, 10, 10, 0.5f);
    batch.pushRect(20, 0, 10, 10, 0.5f);
    assert(batch.ranges().size() == 1);
    assert(batch.ranges()[0].firstVert == 0);
    assert(batch.ranges()[0].vertCount == batch.vertices().size());
    assert(!batch.ranges()[0].hasClip);

    // setClip splits a new range; clearClip splits again. Ranges must tile
    // the vertex vector exactly (contiguous, no gaps/overlap) and carry the
    // right clip rects.
    batch.reset();
    batch.pushRect(0, 0, 10, 10, 0.5f);           // unclipped
    batch.setClip(5, 6, 100, 200);
    batch.pushRect(0, 0, 10, 10, 0.5f);           // clipped
    batch.pushLine(0, 0, 50, 0, 2.0f, 0.5f);      // same clip -> same range
    batch.clearClip();
    batch.pushRect(0, 0, 10, 10, 0.5f);           // unclipped again
    assert(batch.ranges().size() == 3);
    const auto& ranges = batch.ranges();
    assert(!ranges[0].hasClip && ranges[0].firstVert == 0 && ranges[0].vertCount == 6);
    assert(ranges[1].hasClip && ranges[1].firstVert == 6 && ranges[1].vertCount == 12);
    assert(nearlyEqual(ranges[1].clipX, 5) && nearlyEqual(ranges[1].clipY, 6));
    assert(nearlyEqual(ranges[1].clipW, 100) && nearlyEqual(ranges[1].clipH, 200));
    assert(!ranges[2].hasClip && ranges[2].firstVert == 18 && ranges[2].vertCount == 6);
    assert(ranges[2].firstVert + ranges[2].vertCount == batch.vertices().size());

    // reset() clears ranges and clip state too.
    batch.reset();
    assert(batch.ranges().empty());
    batch.pushRect(0, 0, 10, 10, 0.5f);
    assert(batch.ranges().size() == 1 && !batch.ranges()[0].hasClip);

    printf("primitives_test: OK\n");
    return 0;
}
