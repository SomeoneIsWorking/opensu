// video_frame — the core's latest frame in its pixel format (0RGB1555, XRGB8888, RGB565) as RGBA.
// A duplicated frame keeps the one before it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <libretro.h>

namespace opensu::retro {

class VideoFrame {
  public:
    /// A frame from the core, rows `pitch` bytes apart; null `data` repeats the last frame.
    void take(const void* data, unsigned width, unsigned height, std::size_t pitch,
              retro_pixel_format format);

    /// Rows of `width` RGBA pixels, top first.
    [[nodiscard]] std::span<const std::uint8_t> rgba() const {
        return rgba_;
    }
    [[nodiscard]] unsigned width() const {
        return width_;
    }
    [[nodiscard]] unsigned height() const {
        return height_;
    }

    /// Whether a new frame arrived since the last call.
    [[nodiscard]] bool takeFresh();

  private:
    std::vector<std::uint8_t> rgba_;
    unsigned width_{0};
    unsigned height_{0};
    bool fresh_{false};
};

} // namespace opensu::retro
