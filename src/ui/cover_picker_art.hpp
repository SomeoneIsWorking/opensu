// cover_picker_art — the textures of the covers "Choose artwork" shows, decoded on a worker as each
// file arrives and uploaded on the frame thread; dropped when the picker closes or reopens.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include "cover_picker.hpp"
#include "gpu2d/image/bitmap.hpp"
#include "gpu2d/texture.hpp"
#include "image_decoder.hpp"

namespace opensu::ui {

class CoverPickerArt {
  public:
    using Upload = std::function<gpu2d::Texture(const gpu2d::image::Bitmap&)>;

    /// Asks for the picks' files not decoded yet and uploads what has been.
    void load(const CoverPicker& picker, const Upload& upload);
    /// Pick `index`'s texture, or null while it has none.
    [[nodiscard]] const gpu2d::Texture* find(std::size_t index) const;

  private:
    struct Slot {
        std::filesystem::path file;
        bool requested{false};
        std::optional<gpu2d::Texture> texture;
    };

    ImageDecoder decoder_;
    std::uint64_t generation_{0};
    std::vector<Slot> slots_;
};

} // namespace opensu::ui
