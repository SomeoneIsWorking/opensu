// game_match_screen — Match a different game on the interface toolkit: a tall card
// (`ui::tallCard`) over the dimmed page, the Steam apps found as a grid of covers, each over its
// title and year, the game's match now ticked; A keeps one, X searches another name, B closes.
#pragma once

#include <memory>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "game_match_controller.hpp"
#include "ui/screen_images.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class GameMatchScreen final : public gpu2d::ui::Screen {
  public:
    GameMatchScreen(GameMatchController& controller, const ui::Chrome& chrome,
                    ui::ScreenImages& images)
        : Screen{"match", controller.title()}, controller_{controller}, chrome_{chrome},
          images_{images} {
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
    GameMatchController& controller_;
    const ui::Chrome& chrome_;
    ui::ScreenImages& images_;
};

} // namespace opensu::app
