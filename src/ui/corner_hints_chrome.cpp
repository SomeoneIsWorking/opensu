#include "corner_hints_chrome.hpp"

#include <string>
#include <vector>

#include "chrome_panel.hpp"

namespace opensu::ui {

std::unique_ptr<gpu2d::ui::Widget> CornerHintsChrome::build(const gpu2d::ui::Theme& /*theme*/) {
    auto root = std::make_unique<ChromePanel>(ChromePanel::Draw{});
    if (!live_) {
        return root;
    }
    for (const Hud::PromptCell& cell : hud_.promptCells()) {
        const gamepad::Button button = cell.prompt.button;
        root->part(std::make_unique<ChromePart>(std::string{"hints/"} + cell.prompt.key,
                                                [this, button](gpu2d::ui::ActContext& /*context*/) {
                                                    if (press_) {
                                                        press_(button);
                                                    }
                                                }),
                   onCanvas(cell.hit));
    }
    return root;
}

} // namespace opensu::ui
