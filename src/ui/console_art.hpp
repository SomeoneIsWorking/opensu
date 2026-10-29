// console_art — the console art a tile shows, per system, from the one theme the player chose
// (`artwork::ConsoleThemes`): its card and its tab glyph, SVGs as white masks through the icon
// atlas and pictures decoded on the image loader's workers. A system the theme has no art for, or
// whose art cannot be read, shows none; art never comes from another theme.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/image_loader.hpp"
#include "gpu2d/texture.hpp"

#include "console_theme.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

/// How a console tile's card is drawn.
enum class CardLook : std::uint8_t {
    /// No picture: the platform's colours, the name and the count.
    None,
    /// A white drawing, tinted, above the name and count.
    Mask,
    /// A picture in its own colours above the name and count.
    Picture,
    /// A picture as the whole tile.
    Tile,
};

class ConsoleArt {
  public:
    ConsoleArt(IconAtlas& icons, gpu2d::ImageLoader& images) : icons_{icons}, images_{images} {
    }

    /// Shows `theme`'s art for `systems`, or none when `theme` is null. Art already loaded from
    /// the same theme is kept.
    void use(std::shared_ptr<const artwork::ConsoleTheme> theme,
             const std::vector<std::string>& systems);

    /// Asks the image loader for the systems' pictures not asked for yet.
    void request();
    /// Pictures asked for and not yet landed.
    [[nodiscard]] std::size_t pending() const noexcept;

    /// How the system's card is drawn now.
    [[nodiscard]] CardLook look(std::string_view system) const;
    /// The system's card: a mask `pixels` square, or the picture; null while it loads or when
    /// there is none.
    [[nodiscard]] const gpu2d::Texture* card(std::string_view system, int pixels);
    /// The system's glyph for a platform frame's tab, a mask `pixels` square or the picture; null
    /// when there is none.
    [[nodiscard]] const gpu2d::Texture* glyph(std::string_view system, int pixels);

  private:
    struct Entry {
        std::optional<artwork::ThemeArt> card;
        std::optional<artwork::ThemeArt> glyph;
        gpu2d::ImageHandle cardImage;
        gpu2d::ImageHandle glyphImage;
    };

    [[nodiscard]] const Entry* find(std::string_view system) const;
    [[nodiscard]] const gpu2d::Texture* texture(const std::optional<artwork::ThemeArt>& art,
                                                const gpu2d::ImageHandle& image, int pixels);

    IconAtlas& icons_;
    gpu2d::ImageLoader& images_;
    std::shared_ptr<const artwork::ConsoleTheme> theme_;
    std::map<std::string, Entry, std::less<>> entries_;
};

} // namespace opensu::ui
