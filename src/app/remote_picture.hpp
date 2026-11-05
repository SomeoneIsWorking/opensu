// remote_picture — a remote game's newest picture on the window's device, which the shell draws
// letterboxed over black: the decoder's RGBA uploaded, the host frame it shows, whether a presented
// frame has shown it yet, and the newest picture as PNG for the control channel.
#pragma once

#include <cstdint>
#include <optional>

#include <SDL3/SDL_gpu.h>

#include "control_channel.hpp"
#include "display/letterbox.hpp"
#include "gpu2d/texture.hpp"
#include "stream/guest_video.hpp"

namespace opensu::app {

class RemotePicture {
  public:
    explicit RemotePicture(SDL_GPUDevice& device);

    /// Shows `frame` from now on. Throws gpu2d::GpuError when it cannot reach the device.
    void show(const stream::GuestVideo::Frame& frame);
    /// Shows nothing.
    void clear();

    /// The picture for the shell to draw; nothing before the first or after `clear`.
    [[nodiscard]] std::optional<gpu2d::TextureView> view() const;
    /// Where it is drawn in `window`: letterboxed, or all of it while there is none.
    [[nodiscard]] display::Placed placement(display::Size window) const;
    /// The host frame shown since the last call, once; nothing when no new one came.
    [[nodiscard]] std::optional<std::uint32_t> takeFresh();
    /// `newest` (the session's newest picture) as PNG, its number with it; empty for none.
    [[nodiscard]] static FramePng png(const std::optional<stream::GuestVideo::Frame>& newest);

  private:
    SDL_GPUDevice* device_;
    std::optional<gpu2d::Texture> texture_;
    std::uint32_t number_{0};
    bool fresh_{false};
};

} // namespace opensu::app
