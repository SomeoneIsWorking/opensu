// glyph_textures — the white console glyphs drawn in a platform frame's tab, by system. Files are
// recorded as they arrive and become textures on the next load, which needs the GPU device.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/texture.hpp"

namespace opensu::ui {

class GlyphTextures {
  public:
    /// Records the glyph file of a system, replacing a texture already loaded for it.
    void set(std::string_view system, const std::filesystem::path& file);

    /// Reads the files recorded since the last call into textures on `device`.
    void load(SDL_GPUDevice& device);

    /// The system's glyph texture, or null while it has none.
    [[nodiscard]] const gpu2d::Texture* find(std::string_view system) const;

  private:
    struct Entry {
        std::filesystem::path file;
        std::optional<gpu2d::Texture> texture;
        bool loaded{false};
    };

    std::map<std::string, Entry, std::less<>> entries_;
};

} // namespace opensu::ui
