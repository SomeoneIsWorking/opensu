#include "frame_filter.hpp"

#include <cstring>

namespace opensu::retro {
namespace {

constexpr std::uint32_t frameCode[] = {
#include "frame.frag.inc"
};

constexpr std::uint32_t sharpBilinearCode[] = {
#include "sharp_bilinear.frag.inc"
};

constexpr std::uint32_t crtLottesCode[] = {
#include "crt_lottes.frag.inc"
};

/// What `frame_grid.glsl`'s push block holds.
struct GridPush {
    std::array<float, 2> texels;
    std::array<float, 2> prescale;
    std::array<float, 2> band;
    float shade;
};

/// What `crt_lottes.frag`'s push block holds.
struct CrtPush {
    std::array<float, 2> texels;
    CrtLottes params;
};

static_assert(sizeof(GridPush) <= framePushSize && sizeof(CrtPush) <= framePushSize);

template <typename Block> FramePush pushOf(const Block& block) {
    FramePush push;
    std::memcpy(push.bytes.data(), &block, sizeof(block));
    push.size = sizeof(block);
    return push;
}

const PixelGrid& gridOf(settings::Filter filter) {
    switch (filter) {
    case settings::Filter::Scanlines:
        return scanlineGrid;
    case settings::Filter::LcdGrid:
        return lcdGrid;
    case settings::Filter::None:
    case settings::Filter::Crt:
        break;
    }
    return noGrid;
}

} // namespace

std::span<const std::uint32_t> fragmentShader(const settings::VideoSettings& video) {
    if (video.filter == settings::Filter::Crt) {
        return crtLottesCode;
    }
    if (video.sampling == settings::Sampling::SharpBilinear) {
        return sharpBilinearCode;
    }
    return frameCode;
}

FramePush framePush(const settings::VideoSettings& video, std::array<float, 2> texels,
                    std::array<float, 2> prescale) {
    if (video.filter == settings::Filter::Crt) {
        return pushOf(CrtPush{texels, crtLottes});
    }
    const PixelGrid& grid = gridOf(video.filter);
    return pushOf(GridPush{texels, prescale, grid.band, grid.shade});
}

} // namespace opensu::retro
