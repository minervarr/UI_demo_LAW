#pragma once
#include <string>

#include "canvas.hh"
#include "frame_input.hh"
#include "gesture.hh"
#include "layout.hh"
#include "pager.hh"

#include "gray.h"

// GestureRecognizer + Pager: the touch half of the framework.
//
// The recognizer is fed from FrameInput rather than from raw touch events, and
// that is on purpose. It takes (action, x, y, t) — DOWN/MOVE/UP — which a mouse
// produces exactly as well as a finger does, so the same page is drivable with
// a pointer on the desktop and with a thumb on a phone, and the tap/pan/fling
// classification is the library's in both cases. A page that only worked under
// a real finger could not be checked here at all.
//
// The Pager is the other half: swipe between cards, with a settle animation and
// a fling that carries. It owns only a scroll offset in pixels — where each
// card lands is pageRect(i), and what is drawn in it is the app's business.
class GesturesPage {
 public:
    GesturesPage();

    void updateLayout(Rect content, float scale);
    void update(float dt, const FrameInput& in);
    void draw(Canvas& c);

 private:
    GestureRecognizer gestures_;
    Pager             pager_;

    Rect  content_{0, 0, 0, 0};
    Rect  cardArea_{0, 0, 0, 0};
    float scale_ = 1.0f;
    double clock_ = 0.0;

    // The last thing the recognizer said, so the page can show its work.
    std::string lastEvent_ = "waiting for a gesture";
    float       flashT_    = 0.0f;   // fades the readout after each event
    bool        wasDown_   = false;
};
