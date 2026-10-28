#include "screen_images.hpp"

#include "tile_artwork.hpp"

namespace opensu::ui {

void ScreenImages::want(const std::filesystem::path& file) {
    if (file.empty() || images_.contains(file)) {
        return;
    }
    images_.emplace(file, loader_.loadFile(file, gpu2d::Filter::Linear, maxCoverSide));
}

const gpu2d::Texture* ScreenImages::find(const std::filesystem::path& file) const {
    const auto found = images_.find(file);
    return found == images_.end() ? nullptr : found->second.texture();
}

} // namespace opensu::ui
