// remote_touch — the on-screen pad over a remote game, on a touchscreen with no controller: the
// runner's `touch::TouchLayer` on the shell's window. What the fingers hold comes out as a pad's
// events, so the buttons go to the host and the pause button to the Guide by the pad's own path;
// the controls are a texture the shell draws over the picture, uploaded when they change.
#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
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

    /// Shows the panel `text` writes (`touch::panelText`) in place of the pad; empty text, the pad.
    void panel(const std::string& text);
    /// The frame's `fingers` on the controls laid out for `surface`, a panel around the picture at
    /// `picture`, or with `shown` false none of them, every finger let go: the pad events for what
    /// changed, in order.
    [[nodiscard]] std::vector<gamepad::Event> follow(bool shown,
                                                     const touch::WindowSurface& surface,
                                                     const touch::Box& picture,
                                                     std::span<const touch::Finger> fingers);
    /// How much of the window's bottom the controls keep from the picture.
    [[nodiscard]] int band() const {
        return shown_ ? static_cast<int>(layer_.band()) : 0;
    }
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
    touch::Box laidAround_;
    std::string panel_;
    bool panelChanged_{false};
    std::vector<gamepad::Button> held_;
    std::optional<gpu2d::Texture> texture_;
    bool shown_{false};
};

} // namespace opensu::app
