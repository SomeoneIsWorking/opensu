#include "video_frame.hpp"

#include <cstring>
#include <utility>

namespace opensu::retro {
namespace {

/// A 5- or 6-bit channel widened to 8 bits, low bits repeating the high ones.
std::uint8_t widen5(unsigned value) {
    return static_cast<std::uint8_t>((value << 3U) | (value >> 2U));
}

std::uint8_t widen6(unsigned value) {
    return static_cast<std::uint8_t>((value << 2U) | (value >> 4U));
}

void convertRow(const std::uint8_t* in, std::uint8_t* out, unsigned width,
                retro_pixel_format format) {
    for (unsigned x = 0; x < width; ++x) {
        std::uint8_t* pixel = out + static_cast<std::size_t>(x) * 4;
        if (format == RETRO_PIXEL_FORMAT_XRGB8888) {
            std::uint32_t value = 0;
            std::memcpy(&value, in + static_cast<std::size_t>(x) * 4, sizeof(value));
            pixel[0] = static_cast<std::uint8_t>(value >> 16U);
            pixel[1] = static_cast<std::uint8_t>(value >> 8U);
            pixel[2] = static_cast<std::uint8_t>(value);
        } else {
            std::uint16_t value = 0;
            std::memcpy(&value, in + static_cast<std::size_t>(x) * 2, sizeof(value));
            if (format == RETRO_PIXEL_FORMAT_RGB565) {
                pixel[0] = widen5((value >> 11U) & 0x1FU);
                pixel[1] = widen6((value >> 5U) & 0x3FU);
                pixel[2] = widen5(value & 0x1FU);
            } else {
                pixel[0] = widen5((value >> 10U) & 0x1FU);
                pixel[1] = widen5((value >> 5U) & 0x1FU);
                pixel[2] = widen5(value & 0x1FU);
            }
        }
        pixel[3] = 0xFF;
    }
}

/// `from`, `width` by `height`, into `to`, turned `quarterTurns` (1 to 3) times counter-clockwise.
void turn(const std::uint8_t* from, std::uint8_t* to, unsigned width, unsigned height,
          unsigned quarterTurns) {
    const std::size_t turnedWidth = quarterTurns == 2 ? width : height;
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            std::size_t at = 0;
            if (quarterTurns == 1) {
                at = static_cast<std::size_t>(width - 1 - x) * turnedWidth + y;
            } else if (quarterTurns == 2) {
                at = static_cast<std::size_t>(height - 1 - y) * turnedWidth + (width - 1 - x);
            } else {
                at = static_cast<std::size_t>(x) * turnedWidth + (height - 1 - y);
            }
            std::memcpy(to + at * 4, from + (static_cast<std::size_t>(y) * width + x) * 4, 4);
        }
    }
}

} // namespace

void VideoFrame::take(const void* data, unsigned width, unsigned height, std::size_t pitch,
                      retro_pixel_format format, unsigned quarterTurns) {
    if (data == nullptr) {
        return;
    }
    const std::size_t bytes = static_cast<std::size_t>(width) * height * 4;
    rgba_.resize(bytes);
    std::vector<std::uint8_t>& drawn = quarterTurns == 0 ? rgba_ : drawn_;
    drawn.resize(bytes);
    const auto* rows = static_cast<const std::uint8_t*>(data);
    for (unsigned y = 0; y < height; ++y) {
        convertRow(rows + y * pitch, drawn.data() + static_cast<std::size_t>(y) * width * 4, width,
                   format);
    }
    if (quarterTurns != 0) {
        turn(drawn_.data(), rgba_.data(), width, height, quarterTurns);
    }
    const bool sideways = quarterTurns % 2 != 0;
    width_ = sideways ? height : width;
    height_ = sideways ? width : height;
    fresh_ = true;
}

bool VideoFrame::takeFresh() {
    return std::exchange(fresh_, false);
}

} // namespace opensu::retro
