// remote_touch — the on-screen pad over a remote game, on a touchscreen with no controller: the
// runner's `touch::TouchLayer` on the shell's window. What the fingers hold comes out as a pad's
// events, so the buttons go to the host and the pause button to the Guide by the pad's own path;
// the controls are a texture the shell draws over the picture, uploaded when they change.
#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "gamepad/event.hpp"
#include "gpu2d/texture.hpp"
#include "touch/touch_layer.hpp"

namespace opensu::app {

class RemoteTouch {
  public:
    /// The art is read from `art` (assets/touch).
    RemoteTouch(SDL_GPUDevice& device, std::filesystem::path art);

    /// The frame's `fingers` on the controls laid out for `surface`, or with `shown` false none of
    /// them, every finger let go: the pad events for what changed, in order.
    [[nodiscard]] std::vector<gamepad::Event>
    follow(bool shown, const touch::WindowSurface& surface, std::span<const touch::Finger> fingers);
    /// Whether the controls are up, so a finger's pointer events are theirs.
    [[nodiscard]] bool shown() const noexcept {
        return shown_;
    }
    /// The controls to draw; nothing while they are not shown. Throws gpu2d::GpuError when it
    /// cannot reach the device.
    [[nodiscard]] std::optional<gpu2d::TextureView> view();

  private:
    /// The events taking the held buttons from `held_` to what the layer holds now.
    void noteHeld(std::vector<gamepad::Event>& events);

    SDL_GPUDevice* device_;
    touch::TouchLayer layer_;
    touch::WindowSurface laidOut_;
    std::vector<gamepad::Button> held_;
    std::optional<gpu2d::Texture> texture_;
    bool shown_{false};
};

} // namespace opensu::app
