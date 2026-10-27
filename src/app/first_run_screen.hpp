// first_run_screen — the first-run integration chooser on the interface toolkit: a card over the
// dimmed home screen with what it is for, a switch per integration and Done. Back does nothing:
// Done is the way on.
#pragma once

#include <memory>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "first_run_chooser.hpp"

namespace opensu::app {

class FirstRunScreen final : public gpu2d::ui::Screen {
  public:
    explicit FirstRunScreen(FirstRunChooser& controller)
        : Screen{"firstRun", "Choose your integrations"}, controller_{controller} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& /*context*/) override {
        return BackReply::Stay;
    }
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override {
        return {};
    }

  private:
    FirstRunChooser& controller_;
};

} // namespace opensu::app
