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

} // namespace

void VideoFrame::take(const void* data, unsigned width, unsigned height, std::size_t pitch,
                      retro_pixel_format format) {
    if (data == nullptr) {
        return;
    }
    width_ = width;
    height_ = height;
    rgba_.resize(static_cast<std::size_t>(width) * height * 4);
    const auto* rows = static_cast<const std::uint8_t*>(data);
    for (unsigned y = 0; y < height; ++y) {
        convertRow(rows + y * pitch, rgba_.data() + static_cast<std::size_t>(y) * width * 4, width,
                   format);
    }
    fresh_ = true;
}

bool VideoFrame::takeFresh() {
    return std::exchange(fresh_, false);
}

} // namespace opensu::retro
