// guide_menu_screen — the Guide menu on the interface toolkit: a panel down the left edge over the
// dimmed home screen or game, its places in a column and the changes made in place on a card
// beside them, so Right from any place reaches Volume. Power opens its own list over the menu.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "guide_menu_controller.hpp"

namespace opensu::app {

class GuideMenuScreen final : public gpu2d::ui::Screen {
  public:
    GuideMenuScreen(GuideMenuController& controller, GuideMenuController::Landing landing)
        : Screen{"guide", "Guide"}, controller_{controller}, landing_{landing} {
    }

    [[nodiscard]] std::string title() const override {
        return controller_.title();
    }
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] gpu2d::Rect area(const gpu2d::Rect& viewport,
                                   const gpu2d::ui::Theme& theme) const override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override;

  private:
    GuideMenuController& controller_;
    GuideMenuController::Landing landing_;
};

/// Power's list: Sleep, Restart, Shut down and the way out of openSU, over the menu.
class GuidePowerScreen final : public gpu2d::ui::Screen {
  public:
    explicit GuidePowerScreen(GuideMenuController& controller)
        : Screen{"guide.power", "Power"}, controller_{controller} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;

  private:
    GuideMenuController& controller_;
};

} // namespace opensu::app
