#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "../platform/input_state.h"
#include "../gfx/primitives.h"
#include "../text/text_renderer.h"

bool pointInRect(float px, float py, float x, float y, float w, float h);

struct Button {
    float x, y, w, h;

    Button(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}

    bool hovered(const InputState& input) const {
        return pointInRect(input.mouseX, input.mouseY, x, y, w, h);
    }

    bool update(const InputState& input) {
        bool hovered = pointInRect(input.mouseX, input.mouseY, x, y, w, h);
        bool clicked = wasDownLastFrame_ && !input.mouseDown && hovered;
        wasDownLastFrame_ = hovered && input.mouseDown;
        return clicked;
    }

    void draw(PrimitiveBatch& batch, TextRenderer& text, std::string_view label) const {
        batch.pushRoundedRect(x, y, w, h, 6.0f, wasDownLastFrame_ ? 0.35f : 0.55f);
        float textW = text.textWidth(label, 20.0f);
        text.drawText(label, x + (w - textW) * 0.5f, y + h * 0.5f + 7.0f, 20.0f, 0.95f);
    }

   private:
    bool wasDownLastFrame_ = false;
};

struct Toggle {
    float x, y, w, h;
    bool on = false;

    Toggle(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}

    bool hovered(const InputState& input) const {
        return pointInRect(input.mouseX, input.mouseY, x, y, w, h);
    }

    bool update(const InputState& input) {
        bool hovered = pointInRect(input.mouseX, input.mouseY, x, y, w, h);
        bool clicked = wasDownLastFrame_ && !input.mouseDown && hovered;
        wasDownLastFrame_ = hovered && input.mouseDown;
        if (clicked) on = !on;
        return clicked;
    }

    void draw(PrimitiveBatch& batch) const {
        batch.pushRoundedRect(x, y, w, h, h * 0.5f, on ? 0.7f : 0.3f);
        float knobD = h - 8.0f;
        float knobX = on ? (x + w - knobD - 4.0f) : (x + 4.0f);
        batch.pushRoundedRect(knobX, y + 4.0f, knobD, knobD, knobD * 0.5f, 0.95f);
    }

   private:
    bool wasDownLastFrame_ = false;
};

inline float sliderValueFromMouseX(float mouseX, float trackX, float trackW,
                            float minValue, float maxValue) {
    float t = trackW > 0.0f ? (mouseX - trackX) / trackW : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return minValue + (maxValue - minValue) * t;
}

inline int listIndexFromMouseY(float mouseY, float trackY, float rowHeight, int itemCount) {
    if (mouseY < trackY) return -1;
    int idx = (int)((mouseY - trackY) / rowHeight);
    if (idx < 0 || idx >= itemCount) return -1;
    return idx;
}

struct Slider {
    float x, y, w, h;
    float minValue, maxValue;
    float value;

    Slider(float x_, float y_, float w_, float h_, float minValue_, float maxValue_, float value_)
        : x(x_), y(y_), w(w_), h(h_), minValue(minValue_), maxValue(maxValue_), value(value_) {}

    bool hovered(const InputState& input) const {
        return pointInRect(input.mouseX, input.mouseY, x, y, w, h);
    }

    bool update(const InputState& input) {
        bool hovered = pointInRect(input.mouseX, input.mouseY, x, y, w, h);
        if (input.mouseDown && (hovered || dragging_)) {
            dragging_ = true;
            value = sliderValueFromMouseX(input.mouseX, x, w, minValue, maxValue);
            return true;
        }
        dragging_ = false;
        return false;
    }

    void draw(PrimitiveBatch& batch) const {
        batch.pushRoundedRect(x, y, w, h, h * 0.5f, 0.3f);
        float t = (maxValue > minValue) ? (value - minValue) / (maxValue - minValue) : 0.0f;
        float knobD = h + 8.0f;
        float knobX = x + t * w - knobD * 0.5f;
        batch.pushRoundedRect(knobX, y - 4.0f, knobD, knobD, knobD * 0.5f, 0.95f);
    }

   private:
    bool dragging_ = false;
};

struct ListBox {
    float x, y, w, h;
    float rowHeight;
    int itemCount;
    int selectedIndex = -1;

    bool hovered(const InputState& input) const {
        return pointInRect(input.mouseX, input.mouseY, x, y, w, h);
    }

    bool update(const InputState& input) {
        if (!input.mouseWentUp) return false;
        if (!pointInRect(input.mouseX, input.mouseY, x, y, w, h)) return false;
        int idx = listIndexFromMouseY(input.mouseY, y, rowHeight, itemCount);
        if (idx < 0) return false;
        selectedIndex = idx;
        return true;
    }

    void draw(PrimitiveBatch& batch, TextRenderer& text,
             const std::vector<std::string>& labels) const {
        for (int i = 0; i < itemCount; i++) {
            float rowY = y + i * rowHeight;
            float gray = (i == selectedIndex) ? 0.5f : 0.2f;
            batch.pushRect(x, rowY, w, rowHeight - 2.0f, gray);
            text.drawText(labels[(size_t)i], x + 12.0f, rowY + rowHeight * 0.5f + 6.0f, 18.0f, 0.9f);
        }
    }
};
