#include "keyboard_chrome.hpp"

#include <string>

namespace opensu::ui {

void KeyboardChrome::setFrame(const ChromeFrame& frame) noexcept {
    frame_ = frame;
    fade_.follow(keyboard_.isOpen(), frame.now);
}

KeyboardLayout KeyboardChrome::layout() const {
    return layoutKeyboard(Rect{0.0f, 0.0f, frame_.width, frame_.height}, frame_.dp,
                          keyboard_.style());
}

float KeyboardChrome::top() const {
    return keyboard_.isOpen() ? layout().panel.y : frame_.height;
}

std::unique_ptr<gpu2d::ui::Widget> KeyboardChrome::build(const gpu2d::ui::Theme& /*theme*/) {
    auto root = std::make_unique<ChromePanel>([this](gpu2d::ui::Painter& painter) {
        painter_.paint(painter.canvas, keyboard_, Point{frame_.width, frame_.height}, frame_.dp,
                       fade_.look(frame_.now));
    });
    if (!keyboard_.isOpen()) {
        return root;
    }
    const KeyboardLayout keys = layout();
    root->solid(gpu2d::Rect{0.0f, keys.panel.y, frame_.width, frame_.height - keys.panel.y});
    for (std::size_t i = 0; i < keys.keys.size(); ++i) {
        root->part(std::make_unique<ChromePart>(
                       "keyboard/key/" + std::to_string(i),
                       [this, i](gpu2d::ui::ActContext& /*context*/) {
                           keyboard_.focusKey(i);
                           if (pressed_) {
                               pressed_();
                           }
                       },
                       [this, i](gpu2d::ui::ActContext& context) {
                           if (keyboard_.focusKey(i)) {
                               context.sound(gpu2d::ui::Sound::Move);
                           }
                       }),
                   onCanvas(keys.keys[i]));
    }
    return root;
}

} // namespace opensu::ui
