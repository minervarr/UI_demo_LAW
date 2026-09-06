#include "page_plot.h"

#include <cmath>

void PlotPage::updateLayout(Rect content, float scale) {
    content_ = content;
    scale_   = scale;

    const float pad = 20.0f * scale;
    Rect body = content;
    Rect header = dockTop(body, 52.0f * scale);

    reset_.x = header.x + pad;
    reset_.y = header.y + 8.0f * scale;
    reset_.w = 130.0f * scale;
    reset_.h = 36.0f * scale;

    plotRect_ = Rect{body.x + pad, body.y, body.w - 2.0f * pad, body.h - pad};
    view_.viewport().setScreenRect(plotRect_.x, plotRect_.y, plotRect_.w, plotRect_.h);

    if (!initialized_ && plotRect_.w > 1.0f) {
        view_.viewport().reset(8.0);
        initialized_ = true;
    }
}

void PlotPage::update(float dt, const FrameInput& in) {
    time_ += dt;

    if (reset_.update(in)) view_.viewport().reset(8.0);

    const bool over = plotRect_.contains(in.pointerX, in.pointerY);

    // Drag to pan. The viewport works in pixels here and converts internally,
    // so a pan stays exactly under the pointer at any zoom.
    if (in.pointerDown && (over || dragging_)) {
        if (!dragging_) { dragging_ = true; lastX_ = in.pointerX; lastY_ = in.pointerY; }
        view_.viewport().panPixels(in.pointerX - lastX_, in.pointerY - lastY_);
        lastX_ = in.pointerX; lastY_ = in.pointerY;
    } else {
        dragging_ = false;
    }

    // Wheel zooms about the pointer, not about the origin — zooming toward
    // where you are looking is the only behaviour that feels right.
    if (over && in.wheelDelta != 0.0f) {
        const double factor = std::pow(0.85, (double)in.wheelDelta);
        view_.viewport().zoomAbout(in.pointerX, in.pointerY, factor);
    }

    // The trace marker reads the sine at the pointer's world x.
    trace_.on = over;
    if (over) {
        const double wx = view_.viewport().toWorldX(in.pointerX);
        trace_.wx    = wx;
        trace_.wy    = std::sin(wx + (double)time_);
        trace_.valid = true;
    }

    resample();
}

// One sample per screen pixel column: the curve stays smooth however far in the
// viewport is zoomed, which sampling at fixed world steps does not.
void PlotPage::resample() {
    const int n = (int)plotRect_.w;
    if (n < 2) { sine_.clear(); damped_.clear(); poly_.clear(); return; }
    sine_.resize((size_t)n);
    damped_.resize((size_t)n);
    poly_.resize((size_t)n);

    const auto& vp = view_.viewport();
    for (int i = 0; i < n; i++) {
        const double wx = vp.toWorldX(plotRect_.x + (float)i);
        sine_[(size_t)i]   = {wx, std::sin(wx + (double)time_), true};
        damped_[(size_t)i] = {wx, std::exp(-0.15 * std::fabs(wx)) * std::cos(wx * 2.0), true};
        // A cubic, to show a curve that leaves the viewport rather than one
        // that politely stays inside it.
        poly_[(size_t)i]   = {wx, 0.02 * wx * wx * wx - 0.4 * wx, true};
    }
}

void PlotPage::draw(Canvas& c) {
    const float pad = 20.0f * scale_;

    c.text("Plot view", content_.x + pad + reset_.w + 20.0f * scale_,
           reset_.y + reset_.h * 0.5f - 11.0f * scale_, 22.0f * scale_, gray(0.95f));
    c.text("drag to pan, wheel to zoom", content_.x + pad + reset_.w + 20.0f * scale_,
           reset_.y + reset_.h * 0.5f + 12.0f * scale_, 14.0f * scale_, gray(0.75f));
    reset_.draw(c, "Reset view");

    if (sine_.empty()) return;

    // The page is grayscale by policy (gray.h), so the curves are separated by
    // LIGHTNESS rather than by hue — which is also what makes them readable to
    // someone who cannot tell the hues apart.
    const plot::Curve curves[3] = {
        {sine_.data(),   (int)sine_.size(),   gray(0.95f)},
        {damped_.data(), (int)damped_.size(), gray(0.70f)},
        {poly_.data(),   (int)poly_.size(),   gray(0.45f)},
    };

    // PlotView paints its own background, and its defaults are the dark
    // functional palette it ships with. This demo is mid-gray, so the field is
    // lightened to sit in it rather than punching a black hole in the page.
    view_.bg      = gray(0.30f);
    view_.subgrid = gray(0.36f);
    view_.grid    = gray(0.42f);
    view_.axis    = gray(0.60f);
    view_.label   = gray(0.90f);
    view_.trace   = gray(1.00f);
    view_.setClipRect(plotRect_.x, plotRect_.y, plotRect_.w, plotRect_.h);
    view_.draw(c, curves, 3, trace_);
    view_.clearClipRect();
}
