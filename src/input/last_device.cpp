#include "last_device.hpp"

namespace opensu::input {

void LastDevice::notePad(const gamepad::Event& event) noexcept {
    if (event.kind == gamepad::Event::Kind::Button && event.button != gamepad::Button::None) {
        current_ = Device::Pad;
        pointing_ = false;
    }
}

void LastDevice::notePointer(float dx, float dy, bool button, bool touch) noexcept {
    if (dx != 0.0f || dy != 0.0f || button) {
        current_ = touch ? Device::Touch : Device::KeyboardMouse;
        pointing_ = true;
    }
}

std::string_view name(Device device) noexcept {
    switch (device) {
    case Device::Pad:
        return "pad";
    case Device::KeyboardMouse:
        return "keyboard";
    case Device::Touch:
        return "touch";
    }
    return "pad";
}

} // namespace opensu::input
