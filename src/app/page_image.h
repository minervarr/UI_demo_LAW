#pragma once
#include <vector>

#include "canvas.hh"
#include "frame_input.hh"
#include "layout.hh"
#include "texture.hh"

#include "gray.h"
#include "widgets_gray.h"

// Textures and the tone-mapping image pipeline.
//
// The picture is generated rather than decoded, and deliberately: a photograph
// would make this a page about a photograph. What is on screen is a synthetic
// scene with a known linear range — a soft background under 1.0, and specular
// highlights that go far above it — so the exposure slider and the two tone
// modes have something whose right answer you can state.
//
// kClip vs kRolloff is the pair worth seeing. kClip tells the literal truth:
// anything over the top is white, and you find out where the top is by moving
// exposure. kRolloff compresses the highlights so the scene is legible on first
// sight. A viewer wants both — one to look, one to trust.
class ImagePage {
 public:
    static constexpr int kTexW = 384;
    static constexpr int kTexH = 256;
    // Linear light, RGBA32F, with highlights well above 1.0.
    static std::vector<float> scenePixels();

    void setTexture(TextureHandle tex) { tex_ = tex; }
    void setHdr(bool hdrActive) { hdr_ = hdrActive; }

    void updateLayout(Rect content, float scale);
    void update(float dt, const FrameInput& in);
    void draw(Canvas& c);

    bool hoversAnyWidget(const FrameInput& in) const {
        return exposure_.hovered(in) || rolloff_.hovered(in) || clipWarn_.hovered(in);
    }

 private:
    TextureHandle tex_ = kInvalidTexture;
    bool          hdr_ = false;

    Slider exposure_{0, 0, 0, 0, 0.05f, 4.0f, 1.0f};
    Toggle rolloff_{0, 0, 0, 0};
    Toggle clipWarn_{0, 0, 0, 0};

    Rect  content_{0, 0, 0, 0};
    Rect  imageRect_{0, 0, 0, 0};
    float scale_ = 1.0f;
};
