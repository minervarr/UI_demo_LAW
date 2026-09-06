// Desktop test for ScrollArea and UiUnits — plain assert(), no framework, the
// convention this family of repos uses. Neither type touches Vulkan or an OS,
// which is the whole reason they can be tested at all: the behaviour that
// matters (can every control be reached on a screen too small for the page?)
// is arithmetic, and arithmetic can be checked without a compositor.
//
// It exists because the interesting case is the one that is hardest to produce
// by hand — a window shorter than its content — and a scripted resize does not
// reliably get one out of a tiling compositor.
#undef NDEBUG
#include <cassert>
#include <cmath>
#include <cstdio>

#include "scroll_area.h"
#include "ui_units.h"

namespace {

FrameInput idle() { return FrameInput{}; }

// A press, then a move, then a release — what a finger and a mouse both
// produce, and what ScrollArea turns into a drag.
void press(FrameInput& in, float x, float y) {
    in = FrameInput{};
    in.pointerX = x; in.pointerY = y;
    in.pointerDown = true; in.pointerWentDown = true;
}
void moveTo(FrameInput& in, float x, float y) {
    in.pointerWentDown = false;
    in.pointerX = x; in.pointerY = y;
    in.pointerDown = true;
}
void release(FrameInput& in) {
    in.pointerWentDown = false;
    in.pointerDown = false;
    in.pointerWentUp = true;
}

void test_no_overflow_means_no_scroll() {
    ScrollArea s;
    s.setViewport({0, 0, 400, 800});
    s.setContent(400, 600);            // shorter than the viewport
    assert(!s.overflowsY());
    assert(!s.overflowsX());

    FrameInput in = idle();
    in.pointerX = 200; in.pointerY = 400; in.wheelDelta = -5.0f;
    s.update(in, 1.0f / 60.0f, false);
    // Nothing to reveal, so the offset must stay put — a page that fits and
    // still drifts under the wheel looks broken.
    assert(s.offsetY() == 0.0f);
    std::printf("  no-overflow: no scroll  OK\n");
}

void test_wheel_reveals_and_clamps() {
    ScrollArea s;
    s.setViewport({0, 0, 400, 500});
    s.setContent(400, 1500);           // 1000 px below the fold
    assert(s.overflowsY());
    assert(std::fabs(s.maxY() - 1000.0f) < 0.01f);

    FrameInput in = idle();
    in.pointerX = 200; in.pointerY = 250; in.wheelDelta = -1.0f;
    s.update(in, 1.0f / 60.0f, false);
    assert(s.offsetY() < 0.0f);        // content moved up

    // Far past the end, then far past the start: both must stop exactly at the
    // bounds rather than run away.
    for (int i = 0; i < 200; i++) s.update(in, 1.0f / 60.0f, false);
    assert(std::fabs(s.offsetY() + s.maxY()) < 0.01f);
    in.wheelDelta = 1.0f;
    for (int i = 0; i < 400; i++) s.update(in, 1.0f / 60.0f, false);
    assert(std::fabs(s.offsetY()) < 0.01f);
    std::printf("  wheel: reveals and clamps at both ends  OK\n");
}

void test_drag_scrolls_and_respects_slop() {
    ScrollArea s;
    s.setViewport({0, 0, 400, 500});
    s.setContent(400, 1500);

    // A press with a tiny movement is a TAP, not a drag: without the slop
    // threshold every click on empty space would nudge the page.
    FrameInput in;
    press(in, 200, 400);
    s.update(in, 1.0f / 60.0f, false);
    moveTo(in, 201, 398);
    s.update(in, 1.0f / 60.0f, false);
    assert(std::fabs(s.offsetY()) < 0.01f);
    release(in);
    s.update(in, 1.0f / 60.0f, false);

    // A real drag upward reveals content below — the gesture a touch screen
    // has instead of a wheel, and the reason this class exists.
    press(in, 200, 400);
    s.update(in, 1.0f / 60.0f, false);
    moveTo(in, 200, 300);
    s.update(in, 1.0f / 60.0f, false);
    moveTo(in, 200, 200);
    s.update(in, 1.0f / 60.0f, false);
    assert(s.offsetY() < -50.0f);
    std::printf("  drag: slop respected, then scrolls  OK\n");
}

void test_claimed_pointer_does_not_scroll() {
    ScrollArea s;
    s.setViewport({0, 0, 400, 500});
    s.setContent(400, 1500);

    // The page said a widget wants this press — a slider being dragged. The
    // page must not move under it.
    FrameInput in;
    press(in, 200, 400);
    s.update(in, 1.0f / 60.0f, /*pointerClaimed=*/true);
    moveTo(in, 200, 200);
    s.update(in, 1.0f / 60.0f, /*pointerClaimed=*/true);
    assert(std::fabs(s.offsetY()) < 0.01f);
    std::printf("  claimed pointer: page stays put  OK\n");
}

void test_horizontal_wheel_when_only_x_overflows() {
    ScrollArea s;
    s.setViewport({0, 0, 300, 500});
    s.setContent(900, 400);            // wide, but it fits vertically
    assert(s.overflowsX());
    assert(!s.overflowsY());

    FrameInput in = idle();
    in.pointerX = 150; in.pointerY = 250; in.wheelDelta = -1.0f;
    s.update(in, 1.0f / 60.0f, false);
    // A plain wheel has one axis. If it did nothing here the right-hand
    // controls would be unreachable with a mouse.
    assert(s.offsetX() < 0.0f);
    assert(s.offsetY() == 0.0f);
    std::printf("  wheel drives X when only X overflows  OK\n");
}

void test_shrinking_viewport_pulls_content_back() {
    ScrollArea s;
    s.setViewport({0, 0, 400, 500});
    s.setContent(400, 1500);
    FrameInput in = idle();
    in.pointerX = 200; in.pointerY = 250; in.wheelDelta = -1.0f;
    for (int i = 0; i < 200; i++) s.update(in, 1.0f / 60.0f, false);
    assert(std::fabs(s.offsetY() + 1000.0f) < 0.01f);

    // The window grew, so there is less to scroll. The offset must follow, or
    // the page is left scrolled past its own end showing blank space.
    s.setViewport({0, 0, 400, 1400});
    in.wheelDelta = 0.0f;
    s.update(in, 1.0f / 60.0f, false);
    assert(std::fabs(s.offsetY() + 100.0f) < 0.01f);
    std::printf("  viewport growth re-clamps the offset  OK\n");
}

void test_units_are_physical_and_guarded() {
    UiUnits u;
    // Unreported density falls back rather than collapsing the margin to zero.
    u.setDpi(0.0f);
    assert(!u.known());
    assert(std::fabs(u.dpi() - UiUnits::kFallbackDpi) < 0.01f);

    // A real density: 3 mm at 160 dpi is 160 * 3 / 25.4 ~= 18.9 px.
    u.setDpi(160.0f);
    assert(u.known());
    assert(std::fabs(u.mm(3.0f) - 18.897f) < 0.01f);
    assert(std::fabs(u.edge() - u.mm(3.0f)) < 0.01f);

    // The same 3 mm is a DIFFERENT pixel count on a denser panel — which is
    // the entire point of authoring it in millimetres.
    UiUnits phone;
    phone.setDpi(420.0f);
    assert(phone.edge() > u.edge() * 2.0f);

    // Absurd values are rejected, not scaled: a 5000 dpi report would put the
    // margin halfway across the screen.
    UiUnits bad;
    bad.setDpi(5000.0f);
    assert(!bad.known());
    assert(std::fabs(bad.dpi() - UiUnits::kFallbackDpi) < 0.01f);
    std::printf("  units: physical, with a fallback and a sanity guard  OK\n");
}

}  // namespace

int main() {
    std::printf("scroll_area_test\n");
    test_no_overflow_means_no_scroll();
    test_wheel_reveals_and_clamps();
    test_drag_scrolls_and_respects_slop();
    test_claimed_pointer_does_not_scroll();
    test_horizontal_wheel_when_only_x_overflows();
    test_shrinking_viewport_pulls_content_back();
    test_units_are_physical_and_guarded();
    std::printf("scroll_area_test: all passed\n");
    return 0;
}
