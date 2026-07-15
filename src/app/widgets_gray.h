#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "canvas.hh"
#include "frame_input.hh"
#include "gray.h"

// This demo's stateful, grayscale widget set, drawn via vk_canvas's Canvas
// and hit-tested against its FrameInput. Deliberately NOT the library's
// widgets:: helpers: those are stateless touch-first settings rows with the
// library's color palette; these keep this showcase's original grayscale
// look and click semantics. All heavy lifting (SDF rounded rects, MSDF text,
// per-frame input edges) is the library's.
//
// Layout code mutates x/y/w/h in place across frames rather than
// reconstructing the widget: reconstruction would silently reset the private
// click-tracking state (wasDownLastFrame_) every frame and make a completed
// down-then-up click structurally undetectable — a real bug this repo hit
// and fixed once already.

struct Button {
    float x, y, w, h;

    Button(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}

    bool hovered(const FrameInput& in) const {
        return Rect{x, y, w, h}.contains(in.pointerX, in.pointerY);
    }

    bool update(const FrameInput& in) {
        bool over = hovered(in);
        bool clicked = wasDownLastFrame_ && !in.pointerDown && over;
        wasDownLastFrame_ = over && in.pointerDown;
        return clicked;
    }

    void draw(Canvas& c, std::string_view label) const {
        c.rect(x, y, w, h, gray(wasDownLastFrame_ ? 0.35f : 0.55f), 6.0f);
        // Canvas::textCentered takes the text-box TOP (baseline = top + size);
        // the old renderer took the baseline directly — converted here.
        c.textCentered(label, x + w * 0.5f, y + h * 0.5f + 7.0f - 20.0f, 20.0f, gray(0.95f));
    }

   private:
    bool wasDownLastFrame_ = false;
};

struct Toggle {
    float x, y, w, h;
    bool on = false;

    Toggle(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}

    bool hovered(const FrameInput& in) const {
        return Rect{x, y, w, h}.contains(in.pointerX, in.pointerY);
    }

    bool update(const FrameInput& in) {
        bool over = hovered(in);
        bool clicked = wasDownLastFrame_ && !in.pointerDown && over;
        wasDownLastFrame_ = over && in.pointerDown;
        if (clicked) on = !on;
        return clicked;
    }

    void draw(Canvas& c) const {
        c.rect(x, y, w, h, gray(on ? 0.7f : 0.3f), h * 0.5f);
        float knobD = h - 8.0f;
        float knobX = on ? (x + w - knobD - 4.0f) : (x + 4.0f);
        c.rect(knobX, y + 4.0f, knobD, knobD, gray(0.95f), knobD * 0.5f);
    }

   private:
    bool wasDownLastFrame_ = false;
};

inline float sliderValueFromPointerX(float px, float trackX, float trackW,
                                     float minValue, float maxValue) {
    float t = trackW > 0.0f ? (px - trackX) / trackW : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return minValue + (maxValue - minValue) * t;
}

struct Slider {
    float x, y, w, h;
    float minValue, maxValue;
    float value;

    Slider(float x_, float y_, float w_, float h_, float minValue_, float maxValue_, float value_)
        : x(x_), y(y_), w(w_), h(h_), minValue(minValue_), maxValue(maxValue_), value(value_) {}

    bool hovered(const FrameInput& in) const {
        return Rect{x, y, w, h}.contains(in.pointerX, in.pointerY);
    }

    bool update(const FrameInput& in) {
        bool over = hovered(in);
        if (in.pointerDown && (over || dragging_)) {
            dragging_ = true;
            value = sliderValueFromPointerX(in.pointerX, x, w, minValue, maxValue);
            return true;
        }
        dragging_ = false;
        return false;
    }

    void draw(Canvas& c) const {
        c.rect(x, y, w, h, gray(0.3f), h * 0.5f);
        float t = (maxValue > minValue) ? (value - minValue) / (maxValue - minValue) : 0.0f;
        float knobD = h + 8.0f;
        float knobX = x + t * w - knobD * 0.5f;
        c.rect(knobX, y - 4.0f, knobD, knobD, gray(0.95f), knobD * 0.5f);
    }

   private:
    bool dragging_ = false;
};

inline int listIndexFromPointerY(float py, float listY, float rowHeight, int itemCount) {
    if (py < listY || rowHeight <= 0.0f) return -1;
    int idx = (int)((py - listY) / rowHeight);
    if (idx < 0 || idx >= itemCount) return -1;
    return idx;
}

struct ListBox {
    float x, y, w, h;
    float rowHeight;
    int itemCount = 0;
    int selectedIndex = -1;

    bool hovered(const FrameInput& in) const {
        return Rect{x, y, w, h}.contains(in.pointerX, in.pointerY);
    }

    bool update(const FrameInput& in) {
        if (!in.pointerWentUp) return false;
        if (!Rect{x, y, w, h}.contains(in.pointerX, in.pointerY)) return false;
        int idx = listIndexFromPointerY(in.pointerY, y, rowHeight, itemCount);
        if (idx < 0) return false;
        selectedIndex = idx;
        return true;
    }

    void draw(Canvas& c, const std::vector<std::string>& labels) const {
        for (int i = 0; i < itemCount; i++) {
            float rowY = y + i * rowHeight;
            c.rect(x, rowY, w, rowHeight - 2.0f, gray(i == selectedIndex ? 0.5f : 0.2f));
            c.text(labels[(size_t)i], x + 12.0f, rowY + rowHeight * 0.5f + 6.0f - 18.0f,
                   18.0f, gray(0.9f));
        }
    }
};
