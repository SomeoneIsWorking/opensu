// vector_icon — the shipped SVG icons, rasterised at the size they are drawn so they stay sharp
// at any resolution.
#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/texture.hpp"

#include "library/game.hpp"

namespace opensu::ui {

enum class Icon : std::uint8_t { Steam, Epic, Gog };

/// The store's icon, or nothing for a ROM.
[[nodiscard]] std::optional<Icon> iconFor(library::Source source) noexcept;

/// The icon's file under the assets folder.
[[nodiscard]] std::string_view iconFile(Icon icon) noexcept;

/// An SVG as a `pixels`-square RGBA mask: white, its coverage in alpha, centred and scaled to
/// fit. Nothing when the file cannot be read or parsed.
[[nodiscard]] std::optional<std::vector<unsigned char>>
rasteriseMask(const std::filesystem::path& svg, int pixels);

/// Icon textures, one per icon and size, made on first use.
class IconAtlas {
  public:
    /// The icons are read from `assetsDir`.
    explicit IconAtlas(std::filesystem::path assetsDir) : assetsDir_{std::move(assetsDir)} {
    }

    /// Makes the textures on `device` from here on.
    void attach(SDL_GPUDevice& device) noexcept {
        device_ = &device;
    }

    /// The icon as a white mask `pixels` square, to draw tinted; null when it cannot be read.
    [[nodiscard]] const gpu2d::Texture* mask(Icon icon, int pixels);

  private:
    struct Entry {
        Icon icon{};
        int pixels{};
        std::optional<gpu2d::Texture> texture;
    };
    std::filesystem::path assetsDir_;
    SDL_GPUDevice* device_{};
    /// A deque, so a texture handed out stays put as more are made.
    std::deque<Entry> entries_;
};

} // namespace opensu::ui
