#include "page_gestures.h"

#include <cstdio>
#include <cmath>

namespace {
constexpr int kCards = 4;
}

GesturesPage::GesturesPage() {
    pager_.setCount(kCards);

    // The recognizer reports; the page records. Keeping every callback this
    // short is what makes the classification legible — the library decides what
    // a gesture WAS, and nothing here second-guesses it.
    gestures_.onTap = [this](float x, float y) {
        char b[96]; std::snprintf(b, sizeof b, "tap  (%.0f, %.0f)", x, y);
        lastEvent_ = b; flashT_ = 1.0f;
    };
    gestures_.onLongPress = [this](float x, float y) {
        char b[96]; std::snprintf(b, sizeof b, "long press  (%.0f, %.0f)", x, y);
        lastEvent_ = b; flashT_ = 1.0f;
    };
    gestures_.onPanStart = [this](float, float) {
        lastEvent_ = "pan start"; flashT_ = 1.0f;
        pager_.dragStart();
    };
    gestures_.onPanMove = [this](float, float, float totalDx, float) {
        pager_.dragTo(totalDx);
    };
    gestures_.onPanEnd = [this](float vx, float vy) {
        char b[96]; std::snprintf(b, sizeof b, "fling  %.0f, %.0f px/s", vx, vy);
        lastEvent_ = b; flashT_ = 1.0f;
        pager_.dragEnd(vx);
    };
}

void GesturesPage::updateLayout(Rect content, float scale) {
    content_ = content;
    scale_   = scale;

    Rect body = content;
    dockTop(body, 84.0f * scale);        // room for the title + readout
    const float pad = 20.0f * scale;
    cardArea_ = Rect{body.x + pad, body.y, body.w - 2.0f * pad, body.h - 44.0f * scale};
    pager_.setViewport(cardArea_.w, cardArea_.h);
}

void GesturesPage::update(float dt, const FrameInput& in) {
    clock_ += (double)dt;
    if (flashT_ > 0.0f) flashT_ -= dt * 0.6f;

    // FrameInput carries a level (pointerDown) plus edges; GestureRecognizer
    // wants DOWN/MOVE/UP transitions, so the edges are what drive it and the
    // level fills in the MOVEs between them.
    const bool inside = cardArea_.contains(in.pointerX, in.pointerY);
    if (in.pointerWentDown && inside) {
        gestures_.onTouch(GestureRecognizer::DOWN, in.pointerX, in.pointerY, clock_);
        wasDown_ = true;
    } else if (in.pointerWentUp && wasDown_) {
        gestures_.onTouch(GestureRecognizer::UP, in.pointerX, in.pointerY, clock_);
        wasDown_ = false;
    } else if (wasDown_ && in.pointerDown) {
        gestures_.onTouch(GestureRecognizer::MOVE, in.pointerX, in.pointerY, clock_);
    }

    pager_.update(dt);
}

void GesturesPage::draw(Canvas& c) {
    const float pad = 20.0f * scale_;
    c.text("Gestures & pager", content_.x + pad, content_.y + pad,
           24.0f * scale_, gray(0.95f));
    c.text("drag or swipe the cards; tap one; press and hold",
           content_.x + pad, content_.y + pad + 30.0f * scale_,
           15.0f * scale_, gray(0.75f));

    // The readout fades rather than vanishing, so a fling that ends off-screen
    // still leaves its velocity readable for a moment.
    const float a = flashT_ > 0.0f ? (0.45f + 0.55f * flashT_) : 0.45f;
    c.text(lastEvent_, content_.x + pad, content_.y + pad + 52.0f * scale_,
           16.0f * scale_, gray(0.98f, a));

    // Cards. pageRect(i) is where the pager says card i currently sits, scroll
    // and settle animation included; everything drawn in it is the app's.
    c.setClip(cardArea_.x, cardArea_.y, cardArea_.w, cardArea_.h);
    for (int i = 0; i < kCards; i++) {
        Rect r = pager_.pageRect(i);
        r.x += cardArea_.x;
        r.y += cardArea_.y;
        if (r.x + r.w < cardArea_.x || r.x > cardArea_.x + cardArea_.w) continue;

        const float inset = 10.0f * scale_;
        const Rect card{r.x + inset, r.y + inset, r.w - 2.0f * inset, r.h - 2.0f * inset};
        c.rect(card.x, card.y, card.w, card.h,
               gray(i == pager_.current() ? 0.62f : 0.44f), 12.0f * scale_);

        char label[32];
        std::snprintf(label, sizeof label, "%d", i + 1);
        c.textCentered(label, card.x + card.w * 0.5f,
                       card.y + card.h * 0.5f - 34.0f * scale_,
                       68.0f * scale_, gray(0.15f));
        c.textCentered(i == pager_.current() ? "current" : "swipe",
                       card.x + card.w * 0.5f,
                       card.y + card.h * 0.5f + 44.0f * scale_,
                       16.0f * scale_, gray(0.2f));
    }
    c.clearClip();

    // The dots the pager draws itself, from the same scroll offset the cards
    // used — so a half-swipe shows a half-lit dot rather than snapping.
    pager_.drawDots(c, content_.x + content_.w * 0.5f,
                    cardArea_.y + cardArea_.h + 22.0f * scale_,
                    22.0f * scale_, 5.0f * scale_,
                    gray(0.95f), gray(0.35f));
}
