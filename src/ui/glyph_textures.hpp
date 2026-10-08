// glyph_textures — the white console glyphs drawn in a platform frame's tab, by system. Files are
// recorded as they arrive and become textures on the next load, which needs the GL context.
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include "raylib.h"

namespace iideck::ui {

class GlyphTextures {
  public:
    GlyphTextures() = default;
    ~GlyphTextures();
    GlyphTextures(const GlyphTextures&) = delete;
    GlyphTextures& operator=(const GlyphTextures&) = delete;

    /// Records the glyph file of a system, replacing a texture already loaded for it.
    void set(std::string_view system, const std::filesystem::path& file);

    /// Reads the files recorded since the last call into textures.
    void load();

    /// The system's glyph texture, or null while it has none.
    [[nodiscard]] const Texture* find(std::string_view system) const;

  private:
    struct Entry {
        std::filesystem::path file;
        Texture texture{};
        bool loaded{false};
    };

    static void unload(Entry& entry);

    std::map<std::string, Entry, std::less<>> entries_;
};

} // namespace iideck::ui
