// image_view — a picture on an interface-toolkit screen: a file's image cropped to fill a rounded
// box of a fixed aspect, a plain box of the control colour while it loads or when there is none.
#pragma once

#include <filesystem>
#include <utility>

#include "gpu2d/ui/widget.hpp"

#include "screen_images.hpp"

namespace opensu::ui {

class ImageView final : public gpu2d::ui::Widget {
  public:
    /// `aspect` is width over height.
    ImageView(ScreenImages& images, std::filesystem::path file, float aspect)
        : images_{images}, file_{std::move(file)}, aspect_{aspect} {
    }

    [[nodiscard]] gpu2d::ui::Size measure(gpu2d::ui::LayoutContext& context,
                                          float maxWidth) override;
    void paint(gpu2d::ui::Painter& painter) const override;

  private:
    ScreenImages& images_;
    std::filesystem::path file_;
    float aspect_;
};

} // namespace opensu::ui
