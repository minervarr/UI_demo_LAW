#include "page_hdr.h"

#include <cstdio>

std::vector<float> HdrPage::rampPixels() {
    std::vector<float> px((size_t)kRampW * kRampH * 4);
    for (int x = 0; x < kRampW; x++) {
        const float lin = kRampMaxLin * ((float)x / (float)(kRampW - 1));
        for (int y = 0; y < kRampH; y++) {
            float* p = px.data() + ((size_t)y * kRampW + x) * 4;
            p[0] = lin; p[1] = lin; p[2] = lin; p[3] = 1.0f;
        }
    }
    return px;
}

void HdrPage::updateLayout(Rect content, float scale) {
    content_ = content;
    scale_   = scale;

    const float pad  = 24.0f * scale;
    const float rowH = 30.0f * scale;

    headerY_ = content.y + pad;
    // Title, then two readout rows, then the ramp, then the controls. One
    // cursor walks the whole page so draw() and this cannot drift apart.
    float y = headerY_ + rowH * 1.3f + rowH * 3.35f + rowH * 0.5f;

    rampRect_ = Rect{content.x + pad, y, content.w - 2.0f * pad, 72.0f * scale};
    y += rampRect_.h + 34.0f * scale;

    const float ctrlW   = 260.0f * scale;
    const float sliderH = 12.0f * scale;
    y += 20.0f * scale;                       // room for the label above
    whiteNits_.x = content.x + pad; whiteNits_.y = y;
    whiteNits_.w = ctrlW;           whiteNits_.h = sliderH;
    y += 46.0f * scale;
    headroom_.x  = content.x + pad; headroom_.y  = y;
    headroom_.w  = ctrlW;           headroom_.h  = sliderH;
    y += 40.0f * scale;
    clipWarn_.x  = content.x + pad; clipWarn_.y  = y;
    clipWarn_.w  = 64.0f * scale;   clipWarn_.h  = 30.0f * scale;
}

void HdrPage::update(float, const FrameInput& in) {
    whiteNits_.update(in);
    headroom_.update(in);
    clipWarn_.update(in);
}

void HdrPage::draw(Canvas& c) {
    const float pad  = 24.0f * scale_;
    const float rowH = 30.0f * scale_;
    float y = headerY_;

    c.text("HDR output", content_.x + pad, y, 26.0f * scale_, gray(0.95f));
    y += rowH * 1.3f;

    // Three separate lines for three separate facts. The middle one used to be
    // the only one shown, which is how this page claimed HDR on an SDR monitor.
    char line[192];
    std::snprintf(line, sizeof line, "1. asked for:  %s",
                  requested_ ? "Hdr10PQ" : "SdrSrgb (not requested)");
    c.text(line, content_.x + pad, y, 17.0f * scale_, gray(0.9f));
    y += rowH * 0.85f;

    std::snprintf(line, sizeof line, "2. swapchain:  %s%s",
                  outputTargetName(target_), hdr_ ? "  (HDR encode)" : "");
    c.text(line, content_.x + pad, y, 17.0f * scale_, gray(0.9f));
    y += rowH * 0.85f;

    // The one that actually decides what you can see.
    if (!headroomKnown_) {
        c.text("3. display:   headroom unknown here",
               content_.x + pad, y, 17.0f * scale_, gray(0.98f));
        y += rowH * 0.8f;
        c.text("assumed 1.0x, so the stripes stay honest",
               content_.x + pad, y, 14.0f * scale_, gray(0.72f));
    } else if (headroom_.maxValue <= 1.0f) {
        c.text("3. display:   1.0x - no headroom",
               content_.x + pad, y, 17.0f * scale_, gray(0.98f));
        y += rowH * 0.8f;
        c.text("an HDR swapchain here is the compositor tone-mapping",
               content_.x + pad, y, 14.0f * scale_, gray(0.72f));
    } else {
        std::snprintf(line, sizeof line, "3. display:   %.2fx headroom, measured",
                      headroom_.maxValue);
        c.text(line, content_.x + pad, y, 17.0f * scale_, gray(0.98f));
        y += rowH * 0.8f;
        c.text("the ramp past the marker is really brighter",
               content_.x + pad, y, 14.0f * scale_, gray(0.72f));
    }

    // ── the ramp ────────────────────────────────────────────────────────────
    if (tex_ != kInvalidTexture) {
        // kClip tells the literal truth: anything over the top goes white, and
        // you find out where the top is by moving headroom. kRolloff would be
        // kinder to a photograph and wrong for a measuring stick.
        c.setImageTone(/*exposure=*/1.0f, ToneMode::kClip,
                       /*white=*/1.0f, /*clipWarn=*/clipWarn_.on);
        c.setImageHdr(whiteNits_.value, headroom_.value);
        // imageFg, not image. Renderer::draw records the layers in a fixed
        // order — background images, THEN shapes, then foreground images, then
        // text — and this app paints a full-screen mid-gray rect as its page
        // background. That rect is a shape, so a background-layer image sits
        // underneath it and is invisible no matter how correct the texture is.
        // The foreground layer is where UI imagery that belongs above the
        // page's own background goes.
        c.imageFg(tex_, rampRect_.x, rampRect_.y, rampRect_.w, rampRect_.h);
        // Reset, or the next image() call anywhere in the app silently
        // inherits this page's tone and HDR settings.
        c.clearImageTone();
    } else {
        c.rect(rampRect_.x, rampRect_.y, rampRect_.w, rampRect_.h, gray(0.25f), 4.0f);
        c.text("ramp texture unavailable", rampRect_.x + 10.0f * scale_,
               rampRect_.y + rampRect_.h * 0.5f - 9.0f * scale_, 16.0f * scale_, gray(0.7f));
    }

    // Where linear light passes 1.0. Left of the line is everything an SDR
    // panel can show; right of it is the headroom.
    const float whiteX = rampRect_.x + rampRect_.w * (1.0f / kRampMaxLin);
    c.rect(whiteX - 1.0f * scale_, rampRect_.y - 7.0f * scale_,
           2.0f * scale_, rampRect_.h + 14.0f * scale_, gray(0.12f));
    c.text("1.0 = display white", whiteX + 6.0f * scale_,
           rampRect_.y - 7.0f * scale_ - 15.0f * scale_, 14.0f * scale_, gray(0.15f));

    // ── controls ────────────────────────────────────────────────────────────
    std::snprintf(line, sizeof line, "whiteNits  %.0f", whiteNits_.value);
    c.text(line, whiteNits_.x, whiteNits_.y - 20.0f * scale_, 16.0f * scale_, gray(0.9f));
    whiteNits_.draw(c);

    std::snprintf(line, sizeof line, "headroom  %.2fx   (display max %.2fx)",
                  headroom_.value, headroom_.maxValue);
    c.text(line, headroom_.x, headroom_.y - 20.0f * scale_, 16.0f * scale_, gray(0.9f));
    headroom_.draw(c);

    clipWarn_.draw(c);
    c.text("clip-warning stripes", clipWarn_.x + clipWarn_.w + 12.0f * scale_,
           clipWarn_.y + clipWarn_.h * 0.5f - 8.0f * scale_, 16.0f * scale_, gray(0.9f));
}
