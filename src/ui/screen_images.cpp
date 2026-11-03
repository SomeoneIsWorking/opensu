#include "screen_images.hpp"

#include <algorithm>

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

std::size_t ScreenImages::pending() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(images_, [](const auto& entry) {
        return entry.second.pending();
    }));
}

} // namespace opensu::ui
