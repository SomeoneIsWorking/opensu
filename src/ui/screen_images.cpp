#include "screen_images.hpp"

#include <algorithm>
#include <system_error>

#include "tile_artwork.hpp"

namespace opensu::ui {

void ScreenImages::want(const std::filesystem::path& file) {
    if (file.empty()) {
        return;
    }
    std::error_code ec;
    const std::filesystem::file_time_type written = std::filesystem::last_write_time(file, ec);
    const auto found = images_.find(file);
    if (found != images_.end() && found->second.written == written) {
        return;
    }
    images_.insert_or_assign(
        file, Image{loader_.loadFile(file, gpu2d::Filter::Linear, maxCoverSide), written});
}

const gpu2d::Texture* ScreenImages::find(const std::filesystem::path& file) const {
    const auto found = images_.find(file);
    return found == images_.end() ? nullptr : found->second.handle.texture();
}

std::size_t ScreenImages::pending() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(images_, [](const auto& entry) {
        return entry.second.handle.pending();
    }));
}

} // namespace opensu::ui
