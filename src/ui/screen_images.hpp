// screen_images — the pictures interface-toolkit screens show, by file: a widget asks for a file
// as it lays out (a hidden window lays out but never paints), the image loader brings it in, and
// every picture goes when no screen is drawn.
#pragma once

#include <filesystem>
#include <map>

#include "gpu2d/image_loader.hpp"
#include "gpu2d/texture.hpp"

namespace opensu::ui {

class ScreenImages {
  public:
    explicit ScreenImages(gpu2d::ImageLoader& loader) noexcept : loader_{loader} {
    }

    /// Asks for `file` when it was not; an empty path asks for nothing.
    void want(const std::filesystem::path& file);
    /// The texture of `file`; null until it lands or when the file does not decode.
    [[nodiscard]] const gpu2d::Texture* find(const std::filesystem::path& file) const;
    /// Lets every picture go, cancelling those still loading.
    void clear() noexcept {
        images_.clear();
    }

  private:
    gpu2d::ImageLoader& loader_;
    std::map<std::filesystem::path, gpu2d::ImageHandle> images_;
};

} // namespace opensu::ui
