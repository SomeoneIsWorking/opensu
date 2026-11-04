#include <algorithm>

#include "video_codec.hpp"

namespace opensu::stream {
namespace {

/// BT.709's YCbCr to RGB factors, in 16.16 fixed point.
constexpr int redFromRed = 103206;
constexpr int greenFromBlue = 12276;
constexpr int greenFromRed = 30679;
constexpr int blueFromBlue = 121609;
constexpr int half = 1 << 15;

std::uint8_t clamped(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

} // namespace

void rgbaFromYcbcr(const YcbcrPlanes& planes, std::vector<std::uint8_t>& rgba) {
    const auto columns = static_cast<std::size_t>(planes.width);
    const auto rows = static_cast<std::size_t>(planes.height);
    const std::span<const std::uint8_t> luma = planes.luma;
    const std::span<const std::uint8_t> blue = planes.blue;
    const std::span<const std::uint8_t> red = planes.red;
    const std::size_t step = planes.chromaStep;
    rgba.resize(columns * rows * 4);
    for (std::size_t y = 0; y < rows; ++y) {
        const std::size_t lumaRow = y * planes.lumaStride;
        const std::size_t chromaRow = (y / 2) * planes.chromaStride;
        std::uint8_t* out = rgba.data() + y * columns * 4;
        for (std::size_t x = 0; x < columns; ++x) {
            const int lumaValue = luma[lumaRow + x] << 16;
            const std::size_t chroma = chromaRow + (x / 2) * step;
            const int cb = blue[chroma] - 128;
            const int cr = red[chroma] - 128;
            out[0] = clamped((lumaValue + redFromRed * cr + half) >> 16);
            out[1] = clamped((lumaValue - greenFromBlue * cb - greenFromRed * cr + half) >> 16);
            out[2] = clamped((lumaValue + blueFromBlue * cb + half) >> 16);
            out[3] = 255;
            out += 4;
        }
    }
}

} // namespace opensu::stream
