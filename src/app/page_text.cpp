#include "page_text.h"

#include <cstdio>

#include "layout.hh"
#include "msdf.hh"  // FontStyle
#include "gray.h"

const char* kChineseSample = "你好，世界";
const char* kJapaneseSample = "こんにちは世界";
const char* kKoreanSample = "안녕하세요 세계";

void TextPage::draw(Canvas& canvas, Rect area, float uiScaleFactor) {
    // Scrollable: content taller than the window scrolls via the mouse wheel
    // and clips at the page edge (Canvas::setClip clips shape quads and MSDF
    // glyph quads alike) so it never draws over the nav strip.
    canvas.setClip(area.x, area.y, area.w, area.h);
    float contentBottom = area.y;
    ColumnCursor col(area.x + 40.0f * uiScaleFactor,
                     area.y + 40.0f * uiScaleFactor,
                     20.0f * uiScaleFactor);
    // "pt" numbers are nominal DESIGN sizes at UiScale's reference window
    // height, not physical points: every size is multiplied by uiScaleFactor,
    // so the label names the design size and the live pixel size is printed
    // alongside it.
    struct Line { const char* label; float sizePx; FontStyle style; };
    Line lines[] = {
        {"The quick brown fox", 12.0f, FontStyle::Roman},
        {"The quick brown fox", 20.0f, FontStyle::Roman},
        {"The quick brown fox", 32.0f, FontStyle::Roman},
        {"The quick brown fox", 56.0f, FontStyle::Roman},
        {"Bold", 32.0f, FontStyle::Bold},
        {"Italic", 32.0f, FontStyle::Italic},
    };
    // Never let a line run past the right edge: shrink just that line's size
    // so its measured width fits the available span (same width-clamp idea
    // TopNav uses for the tab strip).
    float availW = area.w - 80.0f * uiScaleFactor;
    char buf[128];
    for (auto& line : lines) {
        float scaledSize = line.sizePx * uiScaleFactor;
        std::snprintf(buf, sizeof(buf), "%s - %g pt design (%.0f px now)",
                      line.label, line.sizePx, scaledSize);
        float w = canvas.textWidthStyled(buf, scaledSize, line.style);
        if (w > availW && w > 0.0f) {
            scaledSize *= availW / w;
            std::snprintf(buf, sizeof(buf), "%s - %g pt design (%.0f px now)",
                          line.label, line.sizePx, scaledSize);
        }
        Rect r = col.next(500.0f * uiScaleFactor, scaledSize * 1.4f);
        // Canvas::textStyled takes the text-box TOP (baseline = top + size);
        // the row's top is exactly where the old baseline-minus-size landed.
        canvas.textStyled(buf, r.x, r.y, scaledSize, gray(0.9f), line.style);
        contentBottom = r.y + r.h;
    }
    const char* cjkLines[] = {kChineseSample, kJapaneseSample, kKoreanSample};
    for (const char* cjk : cjkLines) {
        float scaledSize = 28.0f * uiScaleFactor;
        Rect r = col.next(400.0f * uiScaleFactor, scaledSize * 1.4f);
        canvas.text(cjk, r.x, r.y, scaledSize, gray(0.9f));
        contentBottom = r.y + r.h;
    }
    // Measured by drawing: total unscrolled content height including a
    // symmetric bottom pad, consumed by next frame's update() scroll clamp.
    contentWidth_  = area.w;
    contentHeight_ = contentBottom - area.y + 40.0f * uiScaleFactor;
    canvas.clearClip();
}
