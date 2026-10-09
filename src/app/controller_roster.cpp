#include "controller_roster.hpp"

#include <algorithm>

namespace opensu::app {

bool ControllerRoster::note(const gamepad::Event& event) {
    if (event.kind != gamepad::Event::Kind::Button || event.source.empty()) {
        return false;
    }
    std::vector<gamepad::Button>& held = held_[event.source];
    const auto at = std::ranges::find(held, event.button);
    if (event.pressed && at == held.end()) {
        held.push_back(event.button);
        return true;
    }
    if (!event.pressed && at != held.end()) {
        held.erase(at);
        return true;
    }
    return false;
}

std::vector<ControllerStatus> ControllerRoster::controllers(Clock::time_point now) {
    std::vector<ControllerStatus> list;
    std::set<std::string> present;
    for (const gamepad::PadInfo& pad : sources_.pads()) {
        ControllerStatus status;
        status.id = pad.id;
        status.name = pad.name;
        status.player = list.size() + 1;
        if (const auto held = held_.find(pad.id); held != held_.end()) {
            status.held = held->second;
        }
        Reading& reading = batteries_[pad.id];
        if (reading.at == Clock::time_point{} || now - reading.at >= batteryLife) {
            reading.battery = sources_.battery(pad.uniq);
            reading.at = now;
        }
        status.battery = reading.battery;
        present.insert(pad.id);
        list.push_back(std::move(status));
    }
    // A pad that left holds nothing and has no battery to remember.
    std::erase_if(held_, [&present](const auto& entry) {
        return !present.contains(entry.first);
    });
    std::erase_if(batteries_, [&present](const auto& entry) {
        return !present.contains(entry.first);
    });
    return list;
}

} // namespace opensu::app
