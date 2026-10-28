// console_theme — one console art theme: its name, author and licence, and per system a card and
// a tab glyph. A folder theme is `theme.json` with `cards/<system>.<svg|webp|png>` and
// `glyphs/<system>.<svg|webp|png>` beside it (assets/consoles/README.md); iiSU's is read from the
// file the player placed (`IisuPack`). A system the theme has no art for has none: art is never
// taken from another theme.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "iisu_pack.hpp"

namespace opensu::artwork {

/// One picture of a theme.
struct ThemeArt {
    enum class Kind : std::uint8_t {
        /// An SVG, drawn as a white mask and tinted.
        Mask,
        /// An image drawn in its own colours.
        Picture,
    };
    Kind kind{Kind::Picture};
    /// A mask's SVG, or a picture's image file; empty for a picture read through `bytes`.
    std::filesystem::path file;
    /// Names a picture for the image loader.
    std::string key;
    /// A picture's encoded bytes when it is not a file of its own; safe on any thread. Nothing,
    /// with `error` saying why, when it cannot be read.
    std::function<std::optional<std::string>(std::string& error)> bytes;
};

/// How a theme's cards stand on a console tile.
enum class CardStyle : std::uint8_t {
    /// Above the console's name and count on the platform's colours.
    Mark,
    /// As the whole tile, carrying its own frame and name.
    Tile,
};

/// What the settings row shows of a theme.
struct ThemeInfo {
    std::string id;
    std::string name;
    /// One line for the row: whose art it is and its licence.
    std::string note;

    bool operator==(const ThemeInfo&) const = default;
};

class ConsoleTheme {
  public:
    ConsoleTheme(const ConsoleTheme&) = delete;
    ConsoleTheme& operator=(const ConsoleTheme&) = delete;
    ConsoleTheme(ConsoleTheme&&) = delete;
    ConsoleTheme& operator=(ConsoleTheme&&) = delete;
    virtual ~ConsoleTheme() = default;

    [[nodiscard]] const ThemeInfo& info() const noexcept {
        return info_;
    }
    [[nodiscard]] CardStyle cardStyle() const noexcept {
        return style_;
    }
    /// The system's card, or nothing when this theme has none.
    [[nodiscard]] virtual std::optional<ThemeArt> card(std::string_view system) const = 0;
    /// The system's tab glyph, or nothing when this theme has none.
    [[nodiscard]] virtual std::optional<ThemeArt> glyph(std::string_view system) const = 0;

  protected:
    ConsoleTheme(ThemeInfo info, CardStyle style) : info_{std::move(info)}, style_{style} {
    }

  private:
    ThemeInfo info_;
    CardStyle style_;
};

/// A theme folder: `theme.json` and its `cards/` and `glyphs/`.
class FolderTheme final : public ConsoleTheme {
  public:
    /// The theme in `folder` under `id`, its glyphs from `glyphs` (its own `glyphs/` unless the
    /// manifest names another). Nothing, with `error` saying why, for a folder whose manifest is
    /// missing or unreadable.
    [[nodiscard]] static std::shared_ptr<const FolderTheme>
    open(std::string id, const std::filesystem::path& folder, const std::filesystem::path& glyphs,
         std::string& error);

    /// The manifest's `glyphs`: the shipped theme whose glyphs this one uses, or empty.
    [[nodiscard]] static std::string glyphsFrom(const std::filesystem::path& folder);

    [[nodiscard]] std::optional<ThemeArt> card(std::string_view system) const override;
    [[nodiscard]] std::optional<ThemeArt> glyph(std::string_view system) const override;

  private:
    using Files = std::map<std::string, std::filesystem::path, std::less<>>;

    FolderTheme(ThemeInfo info, CardStyle style, Files cards, Files glyphs)
        : ConsoleTheme{std::move(info), style}, cards_{std::move(cards)},
          glyphs_{std::move(glyphs)} {
    }

    Files cards_;
    Files glyphs_;
};

/// iiSU's cards, drawn as whole tiles, and its glyphs, from the file the player placed.
class IisuTheme final : public ConsoleTheme {
  public:
    explicit IisuTheme(std::shared_ptr<const IisuPack> pack);

    [[nodiscard]] const std::filesystem::path& file() const noexcept {
        return pack_->file();
    }
    [[nodiscard]] std::optional<ThemeArt> card(std::string_view system) const override;
    [[nodiscard]] std::optional<ThemeArt> glyph(std::string_view system) const override;

  private:
    std::shared_ptr<const IisuPack> pack_;
};

} // namespace opensu::artwork
