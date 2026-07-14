#include "animated_float.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool nearlyEqual(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main() {
    // Linear ease: exact midpoint at half duration
    AnimatedFloat f(0.0f);
    f.set(10.0f, 2.0f, easeLinear);
    assert(f.isAnimating());
    f.update(1.0f); // halfway through a 2s animation
    assert(nearlyEqual(f.value(), 5.0f));
    assert(f.isAnimating());

    f.update(1.0f); // now fully elapsed
    assert(nearlyEqual(f.value(), 10.0f));
    assert(!f.isAnimating());

    // Overshoot must clamp exactly at the target, not extrapolate past it
    AnimatedFloat g(0.0f);
    g.set(4.0f, 1.0f, easeLinear);
    g.update(5.0f);
    assert(nearlyEqual(g.value(), 4.0f));
    assert(!g.isAnimating());

    // A fresh set() restarts elapsed time from the new current value
    AnimatedFloat h(0.0f);
    h.set(10.0f, 1.0f, easeLinear);
    h.update(0.5f); // value == 5.0
    h.set(20.0f, 1.0f, easeLinear); // now animates 5.0 -> 20.0
    assert(nearlyEqual(h.value(), 5.0f));
    h.update(0.5f);
    assert(nearlyEqual(h.value(), 12.5f));

    printf("animated_float_test: OK\n");
    return 0;
}
