// context_menu_screen — the tile context menu on the interface toolkit: a card in the middle of the
// dimmed home screen, titled with the tile and listing its entries; B closes it.
#pragma once

#include <memory>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "context_menu_controller.hpp"

namespace opensu::app {

class ContextMenuScreen final : public gpu2d::ui::Screen {
  public:
    explicit ContextMenuScreen(ContextMenuController& controller)
        : Screen{"menu", controller.title()}, controller_{controller} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    ContextMenuController& controller_;
};

} // namespace opensu::app
