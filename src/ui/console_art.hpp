// console_art — whose console art a tile shows, per system: iiSU's card and tab glyph, read from
// the iiSU file the player gave openSU (`artwork::IisuPack`) and decoded on the image loader's
// workers, when the player chose iiSU's; openSU's own drawing otherwise, for a system the file has
// no art for (as for PS4), and while iiSU's is still loading.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/image_loader.hpp"
#include "gpu2d/texture.hpp"

#include "iisu_pack.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

class ConsoleArt {
  public:
    ConsoleArt(IconAtlas& icons, gpu2d::ImageLoader& images) noexcept
        : icons_{icons}, images_{images} {
    }

    /// Shows iiSU's art from `pack` for `systems`, or openSU's everywhere when `pack` is null.
    /// Art already loaded from the same file is kept.
    void useIisu(std::shared_ptr<const artwork::IisuPack> pack, std::vector<std::string> systems);

    /// Asks the image loader for the systems' cards and glyphs not asked for yet.
    void request();
    /// Cards and glyphs asked for and not yet landed.
    [[nodiscard]] std::size_t pending() const noexcept;

    /// iiSU's card, drawn as the whole tile; null when openSU's card stands.
    [[nodiscard]] const gpu2d::Texture* iisuCard(std::string_view system) const;
    /// openSU's drawing of the console, `pixels` square.
    [[nodiscard]] const gpu2d::Texture* drawing(std::string_view system, int pixels);
    /// The glyph for a platform frame's tab: iiSU's, or openSU's `pixels` square.
    [[nodiscard]] const gpu2d::Texture* glyph(std::string_view system, int pixels);

  private:
    struct Entry {
        gpu2d::ImageHandle card;
        gpu2d::ImageHandle glyph;
    };

    [[nodiscard]] const Entry* find(std::string_view system) const;

    IconAtlas& icons_;
    gpu2d::ImageLoader& images_;
    std::shared_ptr<const artwork::IisuPack> pack_;
    std::vector<std::string> systems_;
    std::map<std::string, Entry, std::less<>> entries_;
};

} // namespace opensu::ui
