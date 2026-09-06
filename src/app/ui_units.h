#pragma once
#include <algorithm>

// Physical units, for the things that should be the same SIZE everywhere.
//
// Most of this demo is laid out in authored pixels multiplied by a UiScale
// factor, which is derived from the window's height. That is the right tool
// for type and for the proportions between elements: it keeps a design looking
// like itself as a window grows.
//
// It is the wrong tool for the gap between content and the edge of the screen.
// That gap is not a proportion of anything — it is the distance a thumb needs
// so a control near the border can be hit, and the distance a phone's curved
// glass and rounded corners eat before a pixel is properly visible. Both are
// measured in millimetres in the physical world and do not care how tall the
// window is. Scaling them with the window makes them too small on a phone,
// which is exactly the screen where they matter most.
//
// So: edge margins and touch targets are authored in millimetres and converted
// here with the display's real pixel density (Host::displayDpi(), which comes
// from wl_output's physical size on Wayland and DisplayMetrics.xdpi on
// Android). Everything else stays on UiScale.
class UiUnits {
 public:
    // 96 dpi is the fallback, not a default: it is what a desktop has
    // historically been assumed to be, and it is what this reduces to when a
    // compositor declines to report a physical size (a nested or headless one
    // has none to report). A wrong-but-sane density puts the margin slightly
    // off; no fallback at all would put it at zero.
    static constexpr float kFallbackDpi = 96.0f;

    void setDpi(float dpi) {
        // Guard the range rather than trusting the platform. A 0 means "not
        // reported", and the absurd values some devices give for xdpi would
        // put a 3 mm margin halfway across the screen.
        dpi_   = (dpi > 40.0f && dpi < 1200.0f) ? dpi : kFallbackDpi;
        known_ = (dpi > 40.0f && dpi < 1200.0f);
    }

    float dpi()   const { return dpi_; }
    bool  known() const { return known_; }

    // Millimetres to pixels on this display.
    float mm(float millimetres) const { return millimetres * dpi_ / 25.4f; }

    // The margin between the content and the edge of the usable area. Three
    // millimetres: enough that a control at the border is not clipped by a
    // rounded corner and can still be pressed, small enough not to waste a
    // phone's width. Floored in pixels so it does not vanish to nothing if a
    // display ever reports an implausibly low density.
    float edge() const { return std::max(6.0f, mm(3.0f)); }

 private:
    float dpi_   = kFallbackDpi;
    bool  known_ = false;
};
