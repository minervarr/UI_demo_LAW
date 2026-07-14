#include "page_text.h"
#include "../ui/layout.h"
#include <cstdio>

const char* kChineseSample = "你好，世界";
const char* kJapaneseSample = "こんにちは世界";
const char* kKoreanSample = "안녕하세요 세계";

void TextPage::update(const InputState& input, Rect area) {
    const float kWheelStepPx = 60.0f;
    scrollY_ = clampScroll(scrollY_ - input.wheelDelta * kWheelStepPx,
                           contentHeight_, area.h);
}

void TextPage::draw(PrimitiveBatch& batch, TextRenderer& text, Rect area, float uiScaleFactor) {
    // NOTE: drawText is UTF-8-aware (see Task 1 of the multi-script plan); the
    // hyphens below are a leftover style choice from before that fix, not a
    // remaining workaround.
    //
    // Scrollable: content taller than the window (e.g. maximized, where the
    // scale cap keeps text from growing but a short window still can't fit
    // every line) scrolls via the mouse wheel and clips at the page edge so
    // it never draws over the nav strip.
    batch.setClip(area.x, area.y, area.w, area.h);
    text.setClip(area.x, area.y, area.w, area.h);
    float contentBottom = area.y;
    ColumnCursor col(area.x + 40.0f * uiScaleFactor,
                     area.y + 40.0f * uiScaleFactor - scrollY_,
                     20.0f * uiScaleFactor);
    // These "pt" numbers are nominal DESIGN sizes at the reference window
    // height (UiScale's referenceHeight, ui/layout.h) -- not real physical
    // points/millimeters. Every size is multiplied by uiScaleFactor below,
    // which scales proportionally with the actual window height (uncapped
    // above), the same "dp/sp"-style responsive-scaling technique this
    // whole app's layout uses (see CLAUDE.md's ui/layout section) rather
    // than a true DPI/metric system. So "32pt" intentionally renders at a
    // different absolute pixel size in a maximized window than in a small
    // one -- the label is naming the DESIGN size, not the current on-screen
    // size. Print the live pixel size alongside it so that distinction is
    // visible instead of silently confusing (the old hardcoded label text
    // never updated at all, which is the bug this was reported as).
    struct Line { const char* label; float sizePx; bool bold; bool italic; };
    Line lines[] = {
        {"The quick brown fox", 12.0f, false, false},
        {"The quick brown fox", 20.0f, false, false},
        {"The quick brown fox", 32.0f, false, false},
        {"The quick brown fox", 56.0f, false, false},
        {"Bold", 32.0f, true, false},
        {"Italic", 32.0f, false, true},
    };
    // Never let a line run past the right edge of the page: the largest
    // design sizes exceed the window width even at 1280px ("…(68 px now)"
    // losing its tail), and worse when maximized. Shrink just that line's
    // size so its measured width fits the available span — the same
    // width-clamp idea TopNav uses for the tab strip.
    float availW = area.w - 80.0f * uiScaleFactor;
    char buf[128];
    for (auto& line : lines) {
        float scaledSize = line.sizePx * uiScaleFactor;
        std::snprintf(buf, sizeof(buf), "%s - %g pt design (%.0f px now)",
                      line.label, line.sizePx, scaledSize);
        float w = text.textWidth(buf, scaledSize);
        if (w > availW && w > 0.0f) {
            scaledSize *= availW / w;
            std::snprintf(buf, sizeof(buf), "%s - %g pt design (%.0f px now)",
                          line.label, line.sizePx, scaledSize);
        }
        Rect r = col.next(500.0f * uiScaleFactor, scaledSize * 1.4f);
        text.drawText(buf, r.x, r.y + scaledSize, scaledSize, 0.9f, line.bold, line.italic);
        contentBottom = r.y + r.h;
    }
    const char* cjkLines[] = {kChineseSample, kJapaneseSample, kKoreanSample};
    for (const char* cjk : cjkLines) {
        float scaledSize = 28.0f * uiScaleFactor;
        Rect r = col.next(400.0f * uiScaleFactor, scaledSize * 1.4f);
        text.drawText(cjk, r.x, r.y + scaledSize, scaledSize, 0.9f, false, false);
        contentBottom = r.y + r.h;
    }
    // Measured by drawing: total unscrolled content height including a
    // symmetric bottom pad, consumed by next frame's update() scroll clamp.
    contentHeight_ = (contentBottom + scrollY_) - area.y + 40.0f * uiScaleFactor;
    batch.clearClip();
    text.clearClip();
}
