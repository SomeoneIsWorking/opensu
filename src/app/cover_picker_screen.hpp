// cover_picker_screen — "Choose artwork" on the interface toolkit: a card over the dimmed details
// page with the game's covers in a grid, each whole in its cell over its label, the one shown now
// ticked; A keeps one, Y connects SteamGridDB while it is not, B closes.
#pragma once

#include <memory>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "cover_picker_controller.hpp"
#include "ui/screen_images.hpp"

namespace opensu::app {

class CoverPickerScreen final : public gpu2d::ui::Screen {
  public:
    CoverPickerScreen(CoverPickerController& controller, ui::ScreenImages& images)
        : Screen{"covers", controller.title()}, controller_{controller}, images_{images} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    CoverPickerController& controller_;
    ui::ScreenImages& images_;
};

} // namespace opensu::app
