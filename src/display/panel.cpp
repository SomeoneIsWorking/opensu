#include "panel.hpp"

#include <cmath>
#include <cstddef>

namespace opensu::display {
namespace {

constexpr std::size_t baseBlockBytes = 128;
constexpr std::size_t screenSizeCm = 21;
constexpr std::size_t firstTiming = 54;
constexpr std::size_t timingBytes = 18;
constexpr std::size_t timings = 4;
constexpr float shapeTolerance = 0.1F;

int twelveBits(std::uint8_t low, int high) noexcept {
    return static_cast<int>(low) | (high << 8);
}

} // namespace

Panel panelOf(std::span<const std::uint8_t> edid, bool builtIn) noexcept {
    Panel panel{.builtIn = builtIn};
    if (edid.size() < baseBlockBytes) {
        return panel;
    }
    for (std::size_t index = 0; index < timings; ++index) {
        const std::span<const std::uint8_t> timing =
            edid.subspan(firstTiming + index * timingBytes, timingBytes);
        // A zero pixel clock marks a display descriptor, not a timing.
        if (timing[0] == 0 && timing[1] == 0) {
            continue;
        }
        const int widthPx = twelveBits(timing[2], timing[4] >> 4);
        const int heightPx = twelveBits(timing[5], timing[7] >> 4);
        const int widthMm = twelveBits(timing[12], timing[14] >> 4);
        const int heightMm = twelveBits(timing[13], timing[14] & 0x0F);
        if (widthPx <= 0 || heightPx <= 0 || widthMm <= 0 || heightMm <= 0) {
            continue;
        }
        const float glass = static_cast<float>(widthMm) / static_cast<float>(heightMm);
        const float mode = static_cast<float>(widthPx) / static_cast<float>(heightPx);
        if (std::abs(glass - mode) <= shapeTolerance) {
            panel.widthMm = widthMm;
            panel.heightMm = heightMm;
            return panel;
        }
    }
    panel.widthMm = static_cast<int>(edid[screenSizeCm]) * 10;
    panel.heightMm = static_cast<int>(edid[screenSizeCm + 1]) * 10;
    return panel;
}

} // namespace opensu::display
