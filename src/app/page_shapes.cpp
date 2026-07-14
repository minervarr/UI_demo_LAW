#include "page_shapes.h"
#include "../ui/layout.h"

void ShapesPage::draw(PrimitiveBatch& batch, Rect area, float uiScaleFactor) {
    float pad = 40.0f * uiScaleFactor;
    ColumnCursor col(area.x + pad, area.y + pad, 20.0f * uiScaleFactor);

    Rect row1 = col.next(area.w - pad * 2, 100.0f * uiScaleFactor);
    RowCursor shapesRow(row1.x, row1.y, 20.0f * uiScaleFactor);
    Rect rectR = shapesRow.next(160.0f * uiScaleFactor, row1.h);
    batch.pushRect(rectR.x, rectR.y, rectR.w, rectR.h, 0.85f);
    Rect roundedR = shapesRow.next(160.0f * uiScaleFactor, row1.h);
    batch.pushRoundedRect(roundedR.x, roundedR.y, roundedR.w, roundedR.h, 20.0f * uiScaleFactor, 0.85f);
    Rect lineR = shapesRow.next(160.0f * uiScaleFactor, row1.h);
    batch.pushLine(lineR.x, lineR.y, lineR.x + lineR.w, lineR.y + lineR.h, 4.0f * uiScaleFactor, 0.85f);
    Rect bezierR = shapesRow.next(200.0f * uiScaleFactor, row1.h);
    batch.pushBezier(bezierR.x, bezierR.y + bezierR.h,
                     bezierR.x + bezierR.w * 0.2f, bezierR.y,
                     bezierR.x + bezierR.w * 0.8f, bezierR.y,
                     bezierR.x + bezierR.w, bezierR.y + bezierR.h,
                     4.0f * uiScaleFactor, 0.85f);

    Rect row2 = col.next(area.w - pad * 2, 80.0f * uiScaleFactor);
    RowCursor gradientRow(row2.x, row2.y, 0.0f);
    for (int i = 0; i < 8; i++) {
        float gray = (float)i / 7.0f;
        Rect g = gradientRow.next((row2.w) / 8.0f, row2.h);
        batch.pushRect(g.x, g.y, g.w * 0.9f, g.h, gray);
    }
}
