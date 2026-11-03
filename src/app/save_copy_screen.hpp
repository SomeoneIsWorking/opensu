// save_copy_screen — "Copy saves" on the interface toolkit: a tall card (`ui::tallCard`) over the
// dimmed details page with the game's title, the copy from and copy to choices (A lists each
// emulator's saves with their date and size), Copy saves and Undo copy, each asking first; B
// closes.
#pragma once

#include <memory>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "save_copy_controller.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class SaveCopyScreen final : public gpu2d::ui::Screen {
  public:
    SaveCopyScreen(SaveCopyController& controller, const ui::Chrome& chrome)
        : Screen{"saves", "Copy saves"}, controller_{controller}, chrome_{chrome} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    SaveCopyController& controller_;
    const ui::Chrome& chrome_;
};

} // namespace opensu::app
