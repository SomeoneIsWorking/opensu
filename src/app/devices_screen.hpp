// devices_screen — the Devices screen on the interface toolkit: the tabs down the side and the
// chosen one's rows beside them, each row the standard widget of its kind.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "devices_controller.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class DevicesScreen final : public gpu2d::ui::Screen {
  public:
    DevicesScreen(DevicesController& controller, const ui::Chrome& chrome)
        : Screen{"devices", "Devices"}, controller_{controller}, chrome_{chrome} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override {
        return tabId();
    }

    /// The id of the chosen tab's widget.
    [[nodiscard]] std::string tabId() const;

  private:
    DevicesController& controller_;
    const ui::Chrome& chrome_;
};

} // namespace opensu::app
