// layout_picker_screen — the Library options panel on the interface toolkit: a card over the dimmed
// home screen with the three layout cards side by side, each drawing its layout, the one in use
// ticked, then the option rows; the card scrolls when it is taller than the window. B and START
// close it.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "layout_picker.hpp"

namespace opensu::app {

class LayoutPickerScreen final : public gpu2d::ui::Screen {
  public:
    explicit LayoutPickerScreen(LayoutPicker& controller)
        : Screen{"options", controller.inLibrary() ? "Library options" : "Home options"},
          controller_{controller} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override;

  private:
    LayoutPicker& controller_;
};

} // namespace opensu::app
