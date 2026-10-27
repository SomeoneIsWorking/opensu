// keyboard_chrome — the on-screen keyboard docked along the bottom edge as a chrome screen of the
// navigator: drawn with its fade, and while it is up its keys are parts the pointer focuses and
// presses, and the strip it covers takes every other press.
#pragma once

#include <functional>
#include <memory>

#include "gpu2d/ui/screen.hpp"

#include "chrome_panel.hpp"
#include "on_screen_keyboard.hpp"
#include "on_screen_keyboard_painter.hpp"
#include "panel_fade.hpp"

namespace opensu::ui {

class KeyboardChrome final : public gpu2d::ui::Screen {
  public:
    KeyboardChrome(OnScreenKeyboard& keyboard, const input::Prompts& prompts, InterfaceText& text)
        : Screen{"keyboard", "Keyboard"}, keyboard_{keyboard}, painter_{prompts, text} {
    }

    /// What a press on a key does once it has the focus: the app's A; set once.
    void onPress(std::function<void()> pressed) {
        pressed_ = std::move(pressed);
    }
    /// Before the navigator lays it out; follows the keyboard opening and closing.
    void setFrame(const ChromeFrame& frame) noexcept;
    /// Where the window's room above the keyboard ends, in pixels: its top while it is up, else
    /// the window's bottom.
    [[nodiscard]] float top() const;

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;

  private:
    [[nodiscard]] KeyboardLayout layout() const;

    OnScreenKeyboard& keyboard_;
    OnScreenKeyboardPainter painter_;
    PanelFade fade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    std::function<void()> pressed_;
    ChromeFrame frame_;
};

} // namespace opensu::ui
