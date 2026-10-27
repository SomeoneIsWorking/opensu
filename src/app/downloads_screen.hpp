// downloads_screen — the Downloads screen on the interface toolkit: a card per download with its
// title, where it comes from, its state and figures, a progress bar while it is measured, and the
// buttons its owner allows. iiSU's Active Tasks list (`ye4`) is the model; the screen is openSU's
// own.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "downloads_controller.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class DownloadsScreen final : public gpu2d::ui::Screen {
  public:
    DownloadsScreen(DownloadsController& controller, const ui::Chrome& chrome)
        : Screen{"downloads", "Downloads"}, controller_{controller}, chrome_{chrome} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    DownloadsController& controller_;
    const ui::Chrome& chrome_;
};

/// The id of button `action` on the row `key`.
[[nodiscard]] std::string downloadButtonId(const std::string& key, std::size_t action);

} // namespace opensu::app
