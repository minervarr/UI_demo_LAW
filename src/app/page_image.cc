#include "page_image.h"

#include <cmath>
#include <cstdio>

std::vector<float> ImagePage::scenePixels() {
    std::vector<float> px((size_t)kTexW * kTexH * 4);
    for (int y = 0; y < kTexH; y++) {
        for (int x = 0; x < kTexW; x++) {
            const float u = (float)x / (float)(kTexW - 1);
            const float v = (float)y / (float)(kTexH - 1);

            // A gentle vertical gradient well inside the display range: this is
            // the part that must NOT change when the tone mode does.
            float lin = 0.05f + 0.55f * (1.0f - v);

            // Three specular highlights, each brighter than the last. The top
            // one is 12x display white — far past anything an SDR panel can
            // show, and comfortably inside what an HDR one can.
            const float spots[3][3] = {
                {0.24f, 0.34f,  2.5f},
                {0.52f, 0.52f,  6.0f},
                {0.78f, 0.68f, 12.0f},
            };
            for (const auto& s : spots) {
                const float dx = (u - s[0]) * 1.6f;
                const float dy = (v - s[1]);
                const float d2 = dx * dx + dy * dy;
                lin += s[2] * std::exp(-d2 * 900.0f);
            }

            float* p = px.data() + ((size_t)y * kTexW + x) * 4;
            p[0] = lin; p[1] = lin; p[2] = lin; p[3] = 1.0f;
        }
    }
    return px;
}

void ImagePage::updateLayout(Rect content, float scale) {
    content_ = content;
    scale_   = scale;

    const float pad = 22.0f * scale;
    float y = content.y + pad + 30.0f * scale;

    // Fit the picture to the width, keeping its aspect, but never taller than
    // half the content — a phone in portrait otherwise leaves no room for the
    // controls the page is about.
    float w = content.w - 2.0f * pad;
    float h = w * (float)kTexH / (float)kTexW;
    const float maxH = content.h * 0.45f;
    if (h > maxH) { h = maxH; w = h * (float)kTexW / (float)kTexH; }
    imageRect_ = Rect{content.x + pad, y, w, h};
    y += h + 30.0f * scale;

    const float ctrlW = 260.0f * scale;
    y += 20.0f * scale;
    exposure_.x = content.x + pad; exposure_.y = y;
    exposure_.w = ctrlW;           exposure_.h = 12.0f * scale;
    y += 42.0f * scale;
    rolloff_.x  = content.x + pad; rolloff_.y  = y;
    rolloff_.w  = 64.0f * scale;   rolloff_.h  = 30.0f * scale;
    y += 40.0f * scale;
    clipWarn_.x = content.x + pad; clipWarn_.y = y;
    clipWarn_.w = 64.0f * scale;   clipWarn_.h = 30.0f * scale;
}

void ImagePage::update(float, const FrameInput& in) {
    exposure_.update(in);
    rolloff_.update(in);
    clipWarn_.update(in);
}

void ImagePage::draw(Canvas& c) {
    const float pad = 22.0f * scale_;
    c.text("Images & tone mapping", content_.x + pad, content_.y + pad,
           24.0f * scale_, gray(0.95f));

    if (tex_ != kInvalidTexture) {
        c.setImageTone(exposure_.value,
                       rolloff_.on ? ToneMode::kRolloff : ToneMode::kClip,
                       /*white=*/1.0f, /*clipWarn=*/clipWarn_.on);
        // Only meaningful under an HDR target; harmless otherwise, since the
        // SDR encode ignores both. Passing the honest headroom is what stops
        // clipWarn striping highlights an HDR panel can genuinely show.
        c.setImageHdr(/*whiteNits=*/203.0f, /*headroom=*/hdr_ ? 4.0f : 1.0f);
        // imageFg for the same reason as the HDR page: the page background is a
        // full-screen shape, and shapes are recorded after background images.
        c.imageFg(tex_, imageRect_.x, imageRect_.y, imageRect_.w, imageRect_.h);
        c.clearImageTone();
    } else {
        c.rect(imageRect_.x, imageRect_.y, imageRect_.w, imageRect_.h, gray(0.25f), 4.0f);
    }

    char line[128];
    std::snprintf(line, sizeof line, "exposure  %.2fx", exposure_.value);
    c.text(line, exposure_.x, exposure_.y - 20.0f * scale_, 16.0f * scale_, gray(0.9f));
    exposure_.draw(c);

    rolloff_.draw(c);
    c.text(rolloff_.on ? "tone: kRolloff (highlights compressed)"
                       : "tone: kClip (literal truth)",
           rolloff_.x + rolloff_.w + 12.0f * scale_,
           rolloff_.y + rolloff_.h * 0.5f - 8.0f * scale_, 16.0f * scale_, gray(0.9f));

    clipWarn_.draw(c);
    c.text("clip-warning stripes", clipWarn_.x + clipWarn_.w + 12.0f * scale_,
           clipWarn_.y + clipWarn_.h * 0.5f - 8.0f * scale_, 16.0f * scale_, gray(0.9f));
}
