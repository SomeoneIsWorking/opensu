// backdrop_blur — the blur behind iiSU's glass (`ea3.n`, an 8 dp backdrop blur): a separable
// Gaussian over the part of the frame a pill covers, drawn clipped to the pill's capsule.
//
// The scene is drawn to a texture first. `prepare` blurs it horizontally into a strip the size of
// the pill; `draw` blurs that vertically onto the frame through the capsule's mask. `prepare` takes
// the scene as a texture and renders to one of its own, so it must run outside any texture mode.
#pragma once

#include "raylib.h"

#include "home_layout.hpp"

namespace opensu::ui {

class BackdropBlur {
  public:
    BackdropBlur() = default;
    ~BackdropBlur();
    BackdropBlur(const BackdropBlur&) = delete;
    BackdropBlur& operator=(const BackdropBlur&) = delete;

    /// Blurs what `scene` (a render texture's, so bottom-up) holds behind `body`, with Gaussian
    /// standard deviation `sigma` pixels.
    void prepare(const Texture& scene, const Rect& body, float sigma);

    /// How the blur is laid on its target.
    struct Placement {
        /// The target's height in pixels, which turns the shader's bottom-up y into the layout's.
        float frameHeight{};
        float opacity{1.0f};
    };

    /// Draws the blur inside the capsule `body` of a target. Does nothing before a `prepare`.
    void draw(const Rect& body, const Placement& placement) const;

  private:
    /// Loads the shaders and the strip on first use; false when the GL objects are unusable.
    bool ready();
    void resize(int width, int height);

    /// A blur shader and where its uniforms are.
    struct Pass {
        Shader shader{};
        int sigma{-1};
        int texel{-1};
        int capsule{-1};
        int frameHeight{-1};
        int opacity{-1};
    };

    Pass horizontal_;
    Pass vertical_;
    RenderTexture2D strip_{};
    bool loaded_{false};
    bool failed_{false};
    bool prepared_{false};
    /// The strip's top edge in frame pixels, and the blur's standard deviation.
    float stripTop_{};
    float sigma_{};
};

} // namespace opensu::ui
