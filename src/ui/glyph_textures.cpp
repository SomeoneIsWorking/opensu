#include "glyph_textures.hpp"

namespace opensu::ui {

GlyphTextures::~GlyphTextures() {
    for (auto& [system, entry] : entries_) {
        unload(entry);
    }
}

void GlyphTextures::unload(Entry& entry) {
    if (entry.loaded && entry.texture.id != 0) {
        UnloadTexture(entry.texture);
    }
    entry.texture = Texture{};
    entry.loaded = false;
}

void GlyphTextures::set(std::string_view system, const std::filesystem::path& file) {
    Entry& entry = entries_[std::string{system}];
    unload(entry);
    entry.file = file;
}

void GlyphTextures::load() {
    for (auto& [system, entry] : entries_) {
        if (entry.loaded) {
            continue;
        }
        entry.loaded = true;
        entry.texture = LoadTexture(entry.file.string().c_str());
        if (entry.texture.id != 0) {
            // The glyph is drawn far smaller than its file, so mipmaps keep it clean.
            GenTextureMipmaps(&entry.texture);
            SetTextureFilter(entry.texture, TEXTURE_FILTER_TRILINEAR);
        }
    }
}

const Texture* GlyphTextures::find(std::string_view system) const {
    const auto found = entries_.find(system);
    return found != entries_.end() && found->second.texture.id != 0 ? &found->second.texture
                                                                    : nullptr;
}

} // namespace opensu::ui
