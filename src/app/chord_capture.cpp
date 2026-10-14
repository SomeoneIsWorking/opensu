#include "chord_capture.hpp"

namespace opensu::app {

void ChordCapture::begin() {
    held_.clear();
    loneB_ = false;
}

std::optional<ChordCapture::Heard> ChordCapture::feed(const gamepad::Event& event) {
    if (event.kind != gamepad::Event::Kind::Button) {
        return std::nullopt;
    }
    if (!event.pressed) {
        std::erase(held_, event.button);
        // B alone cancels on its release, so B can still be held for a chord.
        if (event.button == gamepad::Button::B && loneB_) {
            loneB_ = false;
            return Heard{true, {}};
        }
        return std::nullopt;
    }
    loneB_ = event.button == gamepad::Button::B && held_.empty();
    std::optional<Heard> heard;
    if (!held_.empty()) {
        heard = Heard{false, input::PadChord{held_.front(), event.button}};
    }
    held_.push_back(event.button);
    return heard;
}

} // namespace opensu::app
