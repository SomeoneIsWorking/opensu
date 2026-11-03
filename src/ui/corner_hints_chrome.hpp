// corner_hints_chrome — the HUD's corner prompts as a chrome screen of the navigator: each prompt
// is a part a tap presses, which presses the button it names (iiSU jj2.b's click callback, l7
// case 6: a tapped glyph is that button's press). The HUD draws them.
#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "gpu2d/ui/screen.hpp"

#include "hud.hpp"

namespace opensu::ui {

class CornerHintsChrome final : public gpu2d::ui::Screen {
  public:
    explicit CornerHintsChrome(Hud& hud) : Screen{"hints", "Hints"}, hud_{hud} {
    }

    /// Set once by the app: what a tap on a prompt presses.
    void onPress(std::function<void(gamepad::Button)> press) {
        press_ = std::move(press);
    }
    /// Whether the prompts take the pointer this frame.
    void setLive(bool live) noexcept {
        live_ = live;
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;

  private:
    Hud& hud_;
    std::function<void(gamepad::Button)> press_;
    bool live_{false};
};

} // namespace opensu::ui
