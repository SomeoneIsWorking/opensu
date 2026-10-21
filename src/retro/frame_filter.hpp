// frame_filter — what the frame's pixels are drawn with: the fragment shader for its sampling and
// filter, the built-in filters' parameters, and the push block a draw hands that shader. The
// scanlines and the LCD grid darken bands on the source pixels' edges, so they line up with the
// core's pixels at any scale; the CRT is crt-lottes and does its own sampling.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "settings/video_settings.hpp"

namespace opensu::retro {

/// A grid filter: a dark band at the right and bottom edge of every source pixel.
struct PixelGrid {
    /// The band's width across and down, in source pixels; 0 draws none.
    std::array<float, 2> band;
    /// How much of the light under a band it takes, 0 to 1.
    float shade;
};

inline constexpr PixelGrid noGrid{{0.0F, 0.0F}, 0.0F};
inline constexpr PixelGrid scanlineGrid{{0.0F, 0.5F}, 0.5F};
inline constexpr PixelGrid lcdGrid{{0.25F, 0.25F}, 0.45F};

/// crt-lottes' parameters, in its shader's push block order.
struct CrtLottes {
    float hardScan;
    float hardPix;
    float warpX;
    float warpY;
    float maskDark;
    float maskLight;
    float scaleInLinearGamma;
    float shadowMask;
    float brightBoost;
    float hardBloomScan;
    float hardBloomPix;
    float bloomAmount;
    float shape;
};

/// crt-lottes.slang's defaults.
inline constexpr CrtLottes crtLottes{
    .hardScan = -8.0F,
    .hardPix = -3.0F,
    .warpX = 0.031F,
    .warpY = 0.041F,
    .maskDark = 0.5F,
    .maskLight = 1.5F,
    .scaleInLinearGamma = 1.0F,
    .shadowMask = 3.0F,
    .brightBoost = 1.0F,
    .hardBloomScan = -2.0F,
    .hardBloomPix = -1.5F,
    .bloomAmount = 0.15F,
    .shape = 2.0F,
};

/// The largest push block a frame shader reads, which the pipeline layout reserves.
inline constexpr std::uint32_t framePushSize = 60;

/// The bytes of one draw's push block.
struct FramePush {
    std::array<std::byte, framePushSize> bytes{};
    std::uint32_t size{0};
};

/// The SPIR-V of the fragment shader `video` draws with.
[[nodiscard]] std::span<const std::uint32_t> fragmentShader(const settings::VideoSettings& video);

/// The push block for drawing a frame texture `texels` in size under `video`; `prescale` is sharp
/// bilinear's whole factor per axis.
[[nodiscard]] FramePush framePush(const settings::VideoSettings& video, std::array<float, 2> texels,
                                  std::array<float, 2> prescale);

} // namespace opensu::retro
