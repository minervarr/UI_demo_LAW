#include "widgets.h"
#include "../platform/input_state.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static bool nearlyEqualForTest(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main() {
    assert(pointInRect(50, 50, 0, 0, 100, 100) == true);
    assert(pointInRect(150, 50, 0, 0, 100, 100) == false);
    assert(pointInRect(0, 0, 0, 0, 100, 100) == true);   // inclusive top-left
    assert(pointInRect(100, 100, 0, 0, 100, 100) == false); // exclusive bottom-right

    Button btn{10, 10, 100, 40};
    InputState in{};

    // hovered(): pure hit-test on all four widget types — inside, outside,
    // and the same inclusive-top-left/exclusive-bottom-right edge semantics
    // as pointInRect. Must not depend on any press-tracking state.
    {
        Button hb{10, 10, 100, 40};
        Toggle ht{10, 10, 40, 40};
        Slider hs{10, 10, 100, 20, 0.0f, 1.0f, 0.5f};
        ListBox hl{10, 10, 100, 60, 20.0f, 3};
        InputState hin{};
        hin.mouseX = 20; hin.mouseY = 20;
        assert(hb.hovered(hin) && ht.hovered(hin) && hs.hovered(hin) && hl.hovered(hin));
        hin.mouseX = 500; hin.mouseY = 500;
        assert(!hb.hovered(hin) && !ht.hovered(hin) && !hs.hovered(hin) && !hl.hovered(hin));
        hin.mouseX = 10; hin.mouseY = 10;  // inclusive top-left corner
        assert(hb.hovered(hin));
        hin.mouseX = 110; hin.mouseY = 50; // exclusive bottom-right corner
        assert(!hb.hovered(hin));
    }

    // Frame 1: mouse moves over the button, not pressed — no click yet
    in.mouseX = 50; in.mouseY = 30; in.mouseDown = false;
    assert(btn.update(in) == false);

    // Frame 2: press while hovered — still not a completed click
    in.mouseDown = true;
    assert(btn.update(in) == false);

    // Frame 3: release while still hovered — this is the completed click
    in.mouseDown = false;
    assert(btn.update(in) == true);

    // Frame 4: pressing then releasing OUTSIDE the button must not click it
    Button btn2{10, 10, 100, 40};
    in.mouseX = 500; in.mouseY = 500; in.mouseDown = true;
    assert(btn2.update(in) == false);
    in.mouseDown = false;
    assert(btn2.update(in) == false);

    Toggle tgl{10, 10, 40, 40};
    in.mouseX = 20; in.mouseY = 20; in.mouseDown = true;
    assert(tgl.update(in) == false); // press edge, not yet a completed click
    in.mouseDown = false;
    assert(tgl.update(in) == true);  // release completes the click
    assert(tgl.on == true);
    in.mouseDown = true;
    tgl.update(in);
    in.mouseDown = false;
    assert(tgl.update(in) == true);
    assert(tgl.on == false); // second click flips it back off

    // sliderValueFromMouseX: clamps to [min, max], linear in between
    assert(nearlyEqualForTest(sliderValueFromMouseX(0.0f, 0.0f, 100.0f, 0.0f, 10.0f), 0.0f));
    assert(nearlyEqualForTest(sliderValueFromMouseX(50.0f, 0.0f, 100.0f, 0.0f, 10.0f), 5.0f));
    assert(nearlyEqualForTest(sliderValueFromMouseX(100.0f, 0.0f, 100.0f, 0.0f, 10.0f), 10.0f));
    assert(nearlyEqualForTest(sliderValueFromMouseX(-50.0f, 0.0f, 100.0f, 0.0f, 10.0f), 0.0f)); // clamp low
    assert(nearlyEqualForTest(sliderValueFromMouseX(500.0f, 0.0f, 100.0f, 0.0f, 10.0f), 10.0f)); // clamp high

    // listIndexFromMouseY: row index by integer division, out-of-range -> -1
    assert(listIndexFromMouseY(5.0f, 0.0f, 20.0f, 5) == 0);
    assert(listIndexFromMouseY(25.0f, 0.0f, 20.0f, 5) == 1);
    assert(listIndexFromMouseY(-5.0f, 0.0f, 20.0f, 5) == -1);   // above the list
    assert(listIndexFromMouseY(1000.0f, 0.0f, 20.0f, 5) == -1); // below the last row

    Slider slider{10, 10, 200, 20, 0.0f, 100.0f, 0.0f};
    InputState in2{};
    in2.mouseX = 10; in2.mouseY = 15; in2.mouseDown = true;
    slider.update(in2); // press starts a drag at the track's left edge
    assert(nearlyEqualForTest(slider.value, 0.0f));
    in2.mouseX = 110; // drag to the middle
    slider.update(in2);
    assert(nearlyEqualForTest(slider.value, 50.0f));
    in2.mouseDown = false;
    slider.update(in2); // release ends the drag; value holds at 50
    assert(nearlyEqualForTest(slider.value, 50.0f));

    printf("widgets_test: OK\n");
    return 0;
}
