#include "remote_pad.hpp"

#include <array>
#include <utility>
#include <vector>

#include <linux/input-event-codes.h>

#include "forward_map.hpp"

namespace opensu::gamepad {

RemotePad::RemotePad() : translator_{VirtualPad::capabilities()} {
}

void RemotePad::setMapping(ForwardMap map, std::chrono::milliseconds holdDelay) {
    translator_.setChords(ChordTriggers{.chords = {}, .holdDelay = holdDelay});
    translator_.setForwardMap(std::move(map));
}

void RemotePad::follow(const PadSnapshot& pad) {
    const std::function<std::int32_t(Button)> level = [&pad](Button button) -> std::int32_t {
        if (button == Button::L2) {
            return pad.leftTrigger;
        }
        if (button == Button::R2) {
            return pad.rightTrigger;
        }
        return pad.holds(button) ? 1 : 0;
    };
    std::vector<PadEvent> events;
    const auto put = [this, &events](const PadEvent& event) {
        auto& slot = state_[{event.type, event.code}];
        if (slot != event.value) {
            slot = event.value;
            events.push_back(event);
        }
    };
    for (const Button button : padButtons) {
        if (const std::optional<PadEvent> event = virtualPadEvent(button, level)) {
            put(*event);
        }
    }
    const std::array<std::pair<std::uint16_t, std::int32_t>, 4> sticks{{
        {ABS_X, pad.sticks.leftX},
        {ABS_Y, pad.sticks.leftY},
        {ABS_RX, pad.sticks.rightX},
        {ABS_RY, pad.sticks.rightY},
    }};
    for (const auto& [code, value] : sticks) {
        put(PadEvent{EV_ABS, code, value});
    }
    if (events.empty()) {
        return;
    }
    std::vector<PadEvent> forward;
    std::vector<Event> controls;
    const ChordGate::Clock::time_point now = ChordGate::Clock::now();
    for (const PadEvent& event : events) {
        translator_.translate(event, forward, controls, now);
    }
    if (forward.empty()) {
        return;
    }
    if (forward.back().type != EV_SYN) {
        forward.push_back(PadEvent{EV_SYN, SYN_REPORT, 0});
    }
    pad_.write(forward);
}

void RemotePad::rest() {
    follow(PadSnapshot{});
}

} // namespace opensu::gamepad
