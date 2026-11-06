// details_screen — a game's details page on the interface toolkit, over its wide picture faded
// when the store has one: its cover whole beside the title, the store or system and what is known
// about it, and a pane of the launch button, focused, and the rows that act on it, each row naming
// the copy it acts on; and More, a tall card over it with the rows of the other copies and
// what removes the game.
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

class DetailsMoreScreen final : public gpu2d::ui::Screen {
  public:
    DetailsMoreScreen(DetailsController& controller, const ui::Chrome& chrome)
        : Screen{"details.more", "More"}, controller_{controller}, chrome_{chrome} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    DetailsController& controller_;
    const ui::Chrome& chrome_;
};

} // namespace opensu::app
