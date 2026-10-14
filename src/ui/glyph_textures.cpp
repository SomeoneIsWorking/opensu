#include "glyph_textures.hpp"

#include <string>

#include "gpu2d/image/decode.hpp"

#include "lucent/log.h"

namespace opensu::ui {

void GlyphTextures::set(std::string_view system, const std::filesystem::path& file) {
    Entry& entry = entries_[std::string{system}];
    entry.texture.reset();
    entry.loaded = false;
    entry.file = file;
}

void GlyphTextures::load(SDL_GPUDevice& device) {
    for (auto& [system, entry] : entries_) {
        if (entry.loaded) {
            continue;
        }
        entry.loaded = true;
        std::string error;
        const std::optional<gpu2d::image::Bitmap> pixels =
            gpu2d::image::decodeFile(entry.file, error);
        if (!pixels) {
            lucent::warn("ui", "glyph {} unreadable: {}", entry.file.string(), error);
            continue;
        }
        // The glyph is drawn far smaller than its file, so mipmaps keep it clean.
        entry.texture.emplace(device, pixels->rgba, pixels->width, pixels->height,
                              gpu2d::Filter::Trilinear);
    }
}

const gpu2d::Texture* GlyphTextures::find(std::string_view system) const {
    const auto found = entries_.find(system);
    return found != entries_.end() && found->second.texture ? &*found->second.texture : nullptr;
}

} // namespace opensu::ui
