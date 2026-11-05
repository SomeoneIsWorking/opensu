// guide_menu_screen — the Guide menu on the interface toolkit: a panel down the left edge over the
// dimmed home screen or game, its places in a column and the changes made in place on a card
// beside them, so Right from any place reaches Volume. Power, Invite to play and Players each
// open a list over the menu.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
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

/// A list of the menu's rows over the menu, a card with a title.
class GuideListScreen final : public gpu2d::ui::Screen {
  public:
    GuideListScreen(GuideMenuController& controller, std::string id, std::string title,
                    std::function<std::vector<ui::SettingsRow>()> rows)
        : Screen{std::move(id), std::move(title)}, controller_{controller}, rows_{std::move(rows)} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;

  private:
    GuideMenuController& controller_;
    std::function<std::vector<ui::SettingsRow>()> rows_;
};

} // namespace opensu::app
