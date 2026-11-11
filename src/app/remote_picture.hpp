// remote_picture — a remote game's newest picture on the window's device, which the shell draws
// letterboxed over black: the decoder's RGBA uploaded, or, where the device samples DMA-BUFs, a
// hardware decoder's picture drawn into RGBA on the GPU from where it was decoded (BT.709 full
// range); the host frame it shows, whether a presented frame has shown it yet, and the newest
// picture as PNG for the control channel.
#pragma once

#include <cstdint>
#include <optional>

#include <SDL3/SDL_gpu.h>

#include "control_channel.hpp"
#include "display/letterbox.hpp"
#include "gpu2d/dmabuf_picture.hpp"
#include "gpu2d/texture.hpp"
#include "stream/guest_video.hpp"

namespace opensu::app {

class RemotePicture {
  public:
    /// `importing`: the device samples DMA-BUFs (`ShellWindow::importsDmabuf`).
    RemotePicture(SDL_GPUDevice& device, bool importing);

    /// Shows `frame` from now on; false, with the reason logged and the picture before kept, when
    /// its DMA-BUFs cannot be sampled here, so the session's pictures should be read back. Throws
    /// gpu2d::GpuError when it cannot reach the device.
    [[nodiscard]] bool show(const stream::GuestVideo::Frame& frame);
    /// Shows nothing.
    void clear();

    /// The picture for the shell to draw; nothing before the first or after `clear`.
    [[nodiscard]] std::optional<gpu2d::TextureView> view() const;
    /// Where it is drawn in `window`: letterboxed, or all of it while there is none.
    [[nodiscard]] display::Placed placement(display::Size window, int top, int bottom) const;
    /// Shows the picture at `shape`, width over height; 0 keeps the frames' own.
    void shapeAs(float shape) noexcept {
        shape_ = shape;
    }
    /// The host frame shown since the last call, once; nothing when no new one came.
    [[nodiscard]] std::optional<std::uint32_t> takeFresh();
    /// `newest` (the session's newest picture)'s pixels, its number with it; a picture left in
    /// DMA-BUFs is read back from the one shown, with that one's number. Empty for none.
    [[nodiscard]] FramePixels pixels(const std::optional<stream::GuestVideo::Frame>& newest) const;

  private:
    SDL_GPUDevice* device_;
    std::optional<gpu2d::Texture> texture_;
    std::optional<gpu2d::DmabufPicture> imported_;
    bool showsImported_{false};
    float shape_{0.0F};
    int width_{0};
    int height_{0};
    std::uint32_t number_{0};
    bool fresh_{false};
};

} // namespace opensu::app
