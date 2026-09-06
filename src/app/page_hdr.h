#pragma once
#include <vector>

#include "canvas.hh"
#include "frame_input.hh"
#include "layout.hh"
#include "output_target.hh"
#include "texture.hh"

#include "gray.h"
#include "widgets_gray.h"

// What an HDR swapchain actually bought, stated honestly.
//
// The point of this page is the part an HDR demo usually skips: an HDR request
// is a REQUEST. The driver can refuse it, the compositor can refuse it, and on
// Android the window is only offered the 10-bit pair once the Activity has put
// it into HDR colour mode. None of those refusals announce themselves — a
// fallback to the SDR pin looks exactly like success until you notice that
// nothing is brighter than white. So the app asks Renderer::hdrActive() and
// this page prints the answer, including when the answer is no.
//
// The ramp is drawn as an IMAGE and not as a row of rects, and the engine
// forces that rather than it being a convenience: output_encode.slang's
// encodeUiColor() SATURATES, so shapes, MSDF text and the vector overlay are
// all deliberately clamped to BT.2408 graphics white under an HDR target —
// "UI never blazes". A shape ramp could not show anything above SDR white and
// would look identical on both swapchains, which would make this page a lie.
// encodeImageLinear() is the one path where >1.0 survives.
class HdrPage {
 public:
    // The pixels of the ramp: linear light from 0 to kRampMaxLin, so 1.0 sits
    // at display white and everything past it is the headroom. Handed out so
    // the app can upload it — a page has no Renderer and cannot make textures.
    static constexpr int   kRampW      = 512;
    static constexpr int   kRampH      = 8;
    static constexpr float kRampMaxLin = 8.0f;   // matches the headroom slider
    static std::vector<float> rampPixels();

    void setOutput(OutputTarget target, bool hdrActive) {
        target_ = target;
        hdr_    = hdrActive;
    }
    void setRampTexture(TextureHandle tex) { tex_ = tex; }

    void updateLayout(Rect content, float scale);
    void update(float dt, const FrameInput& in);
    void draw(Canvas& c);

    bool hoversAnyWidget(const FrameInput& in) const {
        return whiteNits_.hovered(in) || headroom_.hovered(in) || clipWarn_.hovered(in);
    }

 private:
    OutputTarget  target_ = OutputTarget::SdrSrgb;
    bool          hdr_    = false;
    TextureHandle tex_    = kInvalidTexture;

    // BT.2408 graphics white is 203 nits, and the range around it is what a UI
    // author actually chooses between — so the slider covers that rather than
    // the panel's full capability.
    Slider whiteNits_{0, 0, 0, 0, 100.0f, 400.0f, 203.0f};
    // How far above display white this target can really reach. It moves only
    // where clipWarn starts striping, so setting it honestly is what stops
    // every legitimately bright pixel being flagged.
    Slider headroom_{0, 0, 0, 0, 1.0f, kRampMaxLin, 4.0f};
    Toggle clipWarn_{0, 0, 0, 0};

    Rect  content_{0, 0, 0, 0};
    Rect  rampRect_{0, 0, 0, 0};
    float scale_    = 1.0f;
    float headerY_  = 0.0f;
};
