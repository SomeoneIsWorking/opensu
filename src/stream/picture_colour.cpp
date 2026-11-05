#include <algorithm>

#include "video_codec.hpp"

namespace opensu::stream {
namespace {

constexpr int half = 1 << 15;
constexpr float fixedOne = 65536.0F;
/// Chroma's zero, as a sample in [0, 1].
constexpr float chromaZero = 128.0F / 255.0F;

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
            out[0] = clamped((lumaValue + bt709.redFromCr * cr + half) >> 16);
            out[1] =
                clamped((lumaValue - bt709.greenFromCb * cb - bt709.greenFromCr * cr + half) >> 16);
            out[2] = clamped((lumaValue + bt709.blueFromCb * cb + half) >> 16);
            out[3] = 255;
            out += 4;
        }
    }
}

gpu2d::YcbcrToRgb rgbFromBt709() noexcept {
    const float red = static_cast<float>(bt709.redFromCr) / fixedOne;
    const float greenCb = static_cast<float>(bt709.greenFromCb) / fixedOne;
    const float greenCr = static_cast<float>(bt709.greenFromCr) / fixedOne;
    const float blue = static_cast<float>(bt709.blueFromCb) / fixedOne;
    return gpu2d::YcbcrToRgb{.red = {1.0F, 0.0F, red, -red * chromaZero},
                             .green = {1.0F, -greenCb, -greenCr, (greenCb + greenCr) * chromaZero},
                             .blue = {1.0F, blue, 0.0F, -blue * chromaZero}};
}

} // namespace opensu::stream
