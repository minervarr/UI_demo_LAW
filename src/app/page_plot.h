#pragma once
#include <vector>

#include "canvas.hh"
#include "frame_input.hh"
#include "layout.hh"
#include "plotview.hh"

#include "gray.h"
#include "widgets_gray.h"

// plotview.hh: axes, gridlines, tick labels and polyline curves, with a world
// viewport you pan and zoom.
//
// The page keeps its own sample buffers and re-evaluates them whenever the
// viewport moves, because plot::Curve holds a POINTER to points the caller
// owns — the view draws, it does not sample. Sampling per visible pixel column
// rather than at fixed world steps is what keeps a curve smooth after a deep
// zoom instead of turning into visible line segments.
class PlotPage {
 public:
    void updateLayout(Rect content, float scale);
    void update(float dt, const FrameInput& in);
    void draw(Canvas& c);

    bool hoversAnyWidget(const FrameInput& in) const { return reset_.hovered(in); }

 private:
    // Split because only one of the three curves moves. Sampling all of them
    // every frame meant ~2000 redundant exp/cos/pow evaluations per frame.
    void resampleAnimated();          // the sine: depends on time_
    void resampleStatic();            // the other two: depend only on the view
    bool viewChanged();               // has the world window moved since last?

    plot::PlotView view_;
    std::vector<plot::CurvePoint> sine_, damped_, poly_;
    plot::TraceMarker trace_;

    Button reset_{0, 0, 0, 0};
    Rect   content_{0, 0, 0, 0};
    Rect   plotRect_{0, 0, 0, 0};
    float  scale_ = 1.0f;
    float  time_  = 0.0f;

    bool  dragging_    = false;
    float lastX_ = 0.0f, lastY_ = 0.0f;
    bool  initialized_ = false;

    // The world window the static curves were last sampled against. They only
    // need redoing when this moves — which a pan or a zoom does and a frame
    // does not.
    double lastXmin_ = 0.0, lastXmax_ = 0.0;
    int    lastN_    = 0;
};
