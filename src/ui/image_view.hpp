// image_view — a picture on an interface-toolkit screen: a file's image cropped to fill a rounded
// box of a fixed aspect, or shown whole inside its frame; a plain box of the control colour while
// it loads or when there is none; and a screen's ground with a picture faded across it.
#pragma once

#include <filesystem>
#include <optional>
#include <utility>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widget.hpp"

#include "screen_images.hpp"

namespace opensu::ui {

class ImageView final : public gpu2d::ui::Widget {
  public:
    /// `aspect` is width over height.
    ImageView(ScreenImages& images, std::filesystem::path file, float aspect)
        : images_{images}, file_{std::move(file)}, aspect_{aspect} {
        images_.want(file_);
    }

    /// Shows the picture whole at its own shape, as large as the frame allows, placed across the
    /// frame by `across` and at its top; the box while it loads keeps the view's aspect.
    ImageView& whole(gpu2d::ui::Align across) noexcept {
        whole_ = across;
        return *this;
    }

    [[nodiscard]] gpu2d::ui::Size measure(gpu2d::ui::LayoutContext& context,
                                          float maxWidth) override;
    void paint(gpu2d::ui::Painter& painter) const override;

  private:
    /// The box a picture `width` x `height` is drawn in.
    [[nodiscard]] gpu2d::Rect box(float width, float height) const noexcept;

    ScreenImages& images_;
    std::filesystem::path file_;
    float aspect_;
    std::optional<gpu2d::ui::Align> whole_;
};

/// The theme's ground with `file` cropped to fill it, faded nearly away, under the children; the
/// bare ground while it loads or when there is none.
class PictureBackdrop final : public gpu2d::ui::Backdrop {
  public:
    PictureBackdrop(ScreenImages& images, std::filesystem::path file)
        : images_{images}, file_{std::move(file)} {
        images_.want(file_);
    }

    void paint(gpu2d::ui::Painter& painter) const override;

  private:
    ScreenImages& images_;
    std::filesystem::path file_;
};

} // namespace opensu::ui
