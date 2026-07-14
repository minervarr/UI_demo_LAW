#pragma once
#include <vector>

// x,y: screen-space position. gray/alpha: color/coverage.
// ax,ay / bx,by / radius / kind: analytic SDF parameters used by the
// fragment shader to antialias non-axis-aligned edges without MSAA:
//   kind 0 (Flat): flat shape, no SDF, hard edges (pushRect's straight,
//     always-pixel-snappable-in-practice sides don't need it). ax,ay,bx,by,
//     radius are unused.
//   kind 1 (Capsule): ax,ay/bx,by are the segment's two endpoints, radius is
//     the half-thickness — draws a round-capped capsule (lines/bezier).
//   kind 2 (Box): ax,ay is the box center, bx,by is the CORE half-extent
//     (the box's half-width/half-height each shrunk by the corner radius),
//     radius is the corner radius — draws a rounded rectangle (or, when the
//     core half-extent is (0,0), a plain circle; a "stadium"/pill shape
//     when only one of the two core half-extents is 0). This is the
//     standard rounded-box SDF (Inigo Quilez's sdRoundBox), the same
//     technique as the capsule SDF extended to axis-aligned rectangles.
enum class SdfKind : int { Flat = 0, Capsule = 1, Box = 2 };
struct Vertex { float x, y; float gray; float alpha; float ax, ay, bx, by; float radius; float kind; };

// A contiguous run of vertices sharing one scissor state. ShapePipeline
// draws each range with its own vkCmdSetScissor (full framebuffer when
// !hasClip), letting scrollable page content clip at its container edge.
struct DrawRange {
    size_t firstVert = 0;
    size_t vertCount = 0;
    bool hasClip = false;
    float clipX = 0, clipY = 0, clipW = 0, clipH = 0;
};

class PrimitiveBatch {
 public:
    void reset() {
        verts_.clear();
        ranges_.clear();
        clipActive_ = false;
    }

    void pushRect(float x, float y, float w, float h, float gray, float alpha = 1.0f);
    // cornerSegments is no longer used (rounded rects are now one antialiased
    // box-SDF quad, not a corner-fan tessellation) — kept as a parameter so
    // existing call sites don't need updating; a future cleanup could drop it.
    void pushRoundedRect(float x, float y, float w, float h, float radius,
                         float gray, float alpha = 1.0f, int cornerSegments = 8);
    void pushLine(float x0, float y0, float x1, float y1, float thickness,
                 float gray, float alpha = 1.0f);
    // segments <= 0 (the default) picks the count adaptively from the
    // control polygon's length (~1 segment per 1.75px on screen, clamped to
    // [8, 256]) so curves stay smooth at any zoom without over-tessellating
    // small ones. An explicit positive count is honored as-is.
    void pushBezier(float x0, float y0, float cx0, float cy0, float cx1, float cy1,
                    float x1, float y1, float thickness, float gray,
                    float alpha = 1.0f, int segments = 0);

    // Subsequent pushes are scissored to this rect at draw time (until
    // clearClip()). Ranges are split lazily on state change; a batch that
    // never clips yields exactly one full-framebuffer range.
    void setClip(float x, float y, float w, float h);
    void clearClip();
    const std::vector<DrawRange>& ranges() const;

    const std::vector<Vertex>& vertices() const { return verts_; }

 private:
    void beginRange();
    // capAx/capAy/capBx/capBy/capRadius: SDF params carried on every vertex
    // of this quad, interpreted per `kind` (see Vertex's comment above).
    void pushQuad(float ax, float ay, float bx, float by,
                 float cx, float cy, float dx, float dy,
                 float gray, float alpha,
                 float capAx = 0.0f, float capAy = 0.0f,
                 float capBx = 0.0f, float capBy = 0.0f,
                 float capRadius = -1.0f,
                 SdfKind kind = SdfKind::Flat);
    std::vector<Vertex> verts_;
    std::vector<DrawRange> ranges_;
    bool clipActive_ = false;
    float clipX_ = 0, clipY_ = 0, clipW_ = 0, clipH_ = 0;
};
