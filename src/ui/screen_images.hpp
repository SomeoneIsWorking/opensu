// screen_images — the pictures interface-toolkit screens show, by file: a widget asks for a file
// as it lays out (a hidden window lays out but never paints), the image loader brings it in, again
// when the file was written since (a cover fitted anew in its place), and every picture goes when
// no screen is drawn.
#pragma once

#include <cstddef>
#include <filesystem>
#include <map>

#include "gpu2d/image_loader.hpp"
#include "gpu2d/texture.hpp"

namespace opensu::ui {

class ScreenImages {
  public:
    explicit ScreenImages(gpu2d::ImageLoader& loader) : loader_{loader} {
    }

    /// Asks for `file` when it was not, or was written since; an empty path asks for nothing.
    void want(const std::filesystem::path& file);
    /// The texture of `file`; null until it lands or when the file does not decode.
    [[nodiscard]] const gpu2d::Texture* find(const std::filesystem::path& file) const;
    /// How many of the asked-for pictures are still loading.
    [[nodiscard]] std::size_t pending() const noexcept;
    /// Lets every picture go, cancelling those still loading.
    void clear() noexcept {
        images_.clear();
    }

  private:
    struct Image {
        gpu2d::ImageHandle handle;
        /// When the file was written as it was asked for.
        std::filesystem::file_time_type written;
    };

    gpu2d::ImageLoader& loader_;
    std::map<std::filesystem::path, Image> images_;
};

} // namespace opensu::ui
