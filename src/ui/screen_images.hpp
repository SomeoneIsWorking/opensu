// screen_images — the pictures interface-toolkit screens show, by file: a widget asks for a file
// as it lays out (a hidden window lays out but never paints), the file is decoded on a worker and
// uploaded on the frame thread, and every texture goes when no screen is drawn.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>

#include "gpu2d/image/bitmap.hpp"
#include "gpu2d/texture.hpp"
#include "image_decoder.hpp"

namespace opensu::ui {

class ScreenImages {
  public:
    using Upload = std::function<gpu2d::Texture(const gpu2d::image::Bitmap&)>;

    /// Asks for `file` when it was not; an empty path asks for nothing.
    void want(const std::filesystem::path& file);
    /// The texture of `file`; null until it is uploaded or when the file does not decode.
    [[nodiscard]] const gpu2d::Texture* find(const std::filesystem::path& file) const;
    /// Uploads what has been decoded.
    void load(const Upload& upload);
    /// Drops every texture and every request.
    void clear();

  private:
    struct Slot {
        std::uint64_t id{0};
        std::optional<gpu2d::Texture> texture;
    };

    ImageDecoder decoder_;
    /// Changes on `clear`, so a decode asked for before it is dropped.
    std::uint64_t generation_{0};
    std::uint64_t next_{0};
    std::map<std::filesystem::path, Slot> slots_;
};

} // namespace opensu::ui
