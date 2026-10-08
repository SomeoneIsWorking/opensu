// vector_icon — the shipped SVG icons, rasterised at the size they are drawn so they stay sharp
// at any resolution.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "raylib.h"

namespace iideck::ui {

enum class Icon : std::uint8_t { Steam, Epic, Gog };

/// The icon's file under the assets folder.
[[nodiscard]] std::string_view iconFile(Icon icon) noexcept;

/// An SVG as a `pixels`-square RGBA mask: white, its coverage in alpha, centred and scaled to
/// fit. Nothing when the file cannot be read or parsed.
[[nodiscard]] std::optional<std::vector<unsigned char>>
rasteriseMask(const std::filesystem::path& svg, int pixels);

/// Icon textures, one per icon and size, made on first use.
class IconAtlas {
  public:
    IconAtlas() = default;
    ~IconAtlas();
    IconAtlas(const IconAtlas&) = delete;
    IconAtlas& operator=(const IconAtlas&) = delete;

    /// The icon as a white mask `pixels` square, to draw tinted; null when it cannot be read.
    [[nodiscard]] const Texture* mask(Icon icon, int pixels);

  private:
    struct Entry {
        Icon icon{};
        int pixels{};
        Texture texture{};
        bool loaded{false};
    };
    std::vector<Entry> entries_;
};

} // namespace iideck::ui
