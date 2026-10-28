// console_art — whose console art a tile shows, per system: iiSU's card and tab glyph when the
// player chose iiSU's and its files are on disk, openSU's own drawing otherwise (as for PS4,
// which iiSU's release has no art for). iiSU's files are recorded whatever the choice, so
// switching back and forth needs no download.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/texture.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

class ConsoleArt {
  public:
    explicit ConsoleArt(IconAtlas& icons) noexcept : icons_{icons} {
    }

    /// Shows iiSU's art where it has some, or openSU's everywhere.
    void useIisu(bool iisu) noexcept;
    [[nodiscard]] bool usingIisu() const noexcept {
        return iisu_;
    }

    /// Records iiSU's card or glyph file of a system, replacing a texture already loaded for it.
    void setIisuCard(std::string_view system, const std::filesystem::path& file);
    void setIisuGlyph(std::string_view system, const std::filesystem::path& file);

    /// Reads the recorded files into textures on `device` while iiSU's art is chosen.
    void load(SDL_GPUDevice& device);
    /// Files recorded but not yet read while iiSU's art is chosen.
    [[nodiscard]] std::size_t pending() const noexcept;

    /// iiSU's card, drawn as the whole tile; null when openSU's card stands.
    [[nodiscard]] const gpu2d::Texture* iisuCard(std::string_view system) const;
    /// openSU's drawing of the console, `pixels` square.
    [[nodiscard]] const gpu2d::Texture* drawing(std::string_view system, int pixels);
    /// The glyph for a platform frame's tab: iiSU's, or openSU's `pixels` square.
    [[nodiscard]] const gpu2d::Texture* glyph(std::string_view system, int pixels);

  private:
    struct Entry {
        std::filesystem::path file;
        std::optional<gpu2d::Texture> texture;
        bool loaded{false};
    };
    using Entries = std::map<std::string, Entry, std::less<>>;

    static void record(Entries& entries, std::string_view system,
                       const std::filesystem::path& file);
    static void load(Entries& entries, SDL_GPUDevice& device);
    [[nodiscard]] const gpu2d::Texture* find(const Entries& entries, std::string_view system) const;

    IconAtlas& icons_;
    bool iisu_{false};
    Entries cards_;
    Entries glyphs_;
};

} // namespace opensu::ui
