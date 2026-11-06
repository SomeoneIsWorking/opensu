// game_backdrop — the running game's last frame on the window's device, read over the runner's
// control channel as a menu opens and drawn behind it, where the window cannot show the game
// through itself (`GameOverlay::seesGame`). A game on no control channel has none.
#pragma once

#include <optional>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/texture.hpp"
#include "retro_session.hpp"

namespace opensu::app {

class GameBackdrop {
  public:
    explicit GameBackdrop(SDL_GPUDevice& device) : device_{&device} {
    }

    /// Reads the frame from `game` when `wanted` turns true, and lets it go when false. Throws
    /// gpu2d::GpuError when it cannot reach the device.
    void follow(bool wanted, RetroSession& game);

    /// The frame for the shell to draw; nothing while none is held.
    [[nodiscard]] std::optional<gpu2d::TextureView> view() const;

  private:
    SDL_GPUDevice* device_;
    bool wanted_{false};
    std::optional<gpu2d::Texture> texture_;
};

} // namespace opensu::app
