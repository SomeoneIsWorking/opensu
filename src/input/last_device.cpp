#include "last_device.hpp"

namespace iideck::input {

void LastDevice::notePad(const gamepad::Event& event) noexcept {
    if (event.kind == gamepad::Event::Kind::Button && event.button != gamepad::Button::None) {
        current_ = Device::Pad;
    }
}

void LastDevice::notePointer(float dx, float dy, bool buttonPressed) noexcept {
    if (dx != 0.0f || dy != 0.0f || buttonPressed) {
        current_ = Device::KeyboardMouse;
    }
}

} // namespace iideck::input
