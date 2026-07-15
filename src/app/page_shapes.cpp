#include "page_shapes.h"

#include "layout.hh"
#include "gray.h"

void ShapesPage::draw(Canvas& canvas, Rect area, float uiScaleFactor) {
    float pad = 40.0f * uiScaleFactor;
    ColumnCursor col(area.x + pad, area.y + pad, 20.0f * uiScaleFactor);

    Rect row1 = col.next(area.w - pad * 2, 100.0f * uiScaleFactor);
    RowCursor shapesRow(row1.x, row1.y, 20.0f * uiScaleFactor);
    Rect rectR = shapesRow.next(160.0f * uiScaleFactor, row1.h);
    canvas.rect(rectR.x, rectR.y, rectR.w, rectR.h, gray(0.85f));
    Rect roundedR = shapesRow.next(160.0f * uiScaleFactor, row1.h);
    canvas.rect(roundedR.x, roundedR.y, roundedR.w, roundedR.h, gray(0.85f),
                20.0f * uiScaleFactor);
    Rect lineR = shapesRow.next(160.0f * uiScaleFactor, row1.h);
    canvas.segment(lineR.x, lineR.y, lineR.x + lineR.w, lineR.y + lineR.h,
                   4.0f * uiScaleFactor, gray(0.85f));
    // Canvas has no bezier primitive; sample the same cubic the old demo drew
    // into a polyline — the library's capsule SDFs antialias each segment, and
    // 32 segments are visually indistinguishable from the analytic curve at
    // this size.
    Rect bezierR = shapesRow.next(200.0f * uiScaleFactor, row1.h);
    {
        float x0 = bezierR.x,                    y0 = bezierR.y + bezierR.h;
        float x1 = bezierR.x + bezierR.w * 0.2f, y1 = bezierR.y;
        float x2 = bezierR.x + bezierR.w * 0.8f, y2 = bezierR.y;
        float x3 = bezierR.x + bezierR.w,        y3 = bezierR.y + bezierR.h;
        constexpr int kSegments = 32;
        float pts[(kSegments + 1) * 2];
        for (int i = 0; i <= kSegments; i++) {
            float t = (float)i / (float)kSegments;
            float u = 1.0f - t;
            float b0 = u * u * u, b1 = 3.0f * u * u * t, b2 = 3.0f * u * t * t, b3 = t * t * t;
            pts[i * 2 + 0] = b0 * x0 + b1 * x1 + b2 * x2 + b3 * x3;
            pts[i * 2 + 1] = b0 * y0 + b1 * y1 + b2 * y2 + b3 * y3;
        }
        canvas.polyline(pts, kSegments + 1, 4.0f * uiScaleFactor, gray(0.85f));
    }

    Rect row2 = col.next(area.w - pad * 2, 80.0f * uiScaleFactor);
    RowCursor gradientRow(row2.x, row2.y, 0.0f);
    for (int i = 0; i < 8; i++) {
        Rect g = gradientRow.next(row2.w / 8.0f, row2.h);
        canvas.rect(g.x, g.y, g.w * 0.9f, g.h, gray((float)i / 7.0f));
    }
}
