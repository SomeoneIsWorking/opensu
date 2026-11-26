// pad_check_screen — the card a calibration ends on: the pad's sticks and triggers drawn live as it
// now plays them (`ui::PadPreview`), with Keep, Calibrate again and, after a calibration, Undo.
// Back keeps.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "gamepad/pad_translator.hpp"

namespace opensu::app {

class PadCheckScreen final : public gpu2d::ui::Screen {
  public:
    struct Hooks {
        /// The pad's sticks and triggers now; nothing once it is gone.
        std::function<std::optional<gamepad::PadAxes>()> axes;
        std::function<void()> keep;
        std::function<void()> again;
        /// Empty when there is no calibration just made to undo.
        std::function<void()> undo;
    };

    /// `line` says how the calibration went.
    PadCheckScreen(std::string id, std::string name, std::string line, Hooks hooks)
        : Screen{std::move(id), "Check " + name}, line_{std::move(line)}, hooks_{std::move(hooks)} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    std::string line_;
    Hooks hooks_;
};

} // namespace opensu::app
