// details_screen — a game's details page on the interface toolkit: its cover whole beside the
// title, the store or system and what is known about it, and a pane of the launch button and the
// rows that act on it, Play focused.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "details_controller.hpp"
#include "ui/screen_images.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class DetailsScreen final : public gpu2d::ui::Screen {
  public:
    DetailsScreen(DetailsController& controller, const ui::Chrome& chrome, ui::ScreenImages& images)
        : Screen{"details", {}}, controller_{controller}, chrome_{chrome}, images_{images} {
    }

    [[nodiscard]] std::string title() const override;
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override;

  private:
    DetailsController& controller_;
    const ui::Chrome& chrome_;
    ui::ScreenImages& images_;
};

} // namespace opensu::app
