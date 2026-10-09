// The controller roster: player order is connection order, held buttons come from the pad named by
// the event, a battery is read once per lifetime, and a pad that leaves is forgotten.
#include <cstdio>

#include "controller_roster.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gamepad::Button;
using test::expect;

gamepad::Event button(const std::string& source, Button which, bool pressed) {
    gamepad::Event event;
    event.kind = gamepad::Event::Kind::Button;
    event.button = which;
    event.pressed = pressed;
    event.source = source;
    return event;
}

struct Rig {
    std::vector<gamepad::PadInfo> pads{{"/dev/input/event5", "Pad One", "aa"},
                                       {"/dev/input/event7", "Pad Two", "bb"}};
    int batteryReads{0};
    app::ControllerRoster roster{app::ControllerRoster::Sources{
        [this] {
            return pads;
        },
        [this](const std::string& uniq) {
            ++batteryReads;
            return uniq == "aa" ? std::optional{device::BatteryStatus{70, false}} : std::nullopt;
        }}};
};

void padsAreInConnectionOrderWithTheirHeldButtons() {
    Rig rig;
    const auto now = app::ControllerRoster::Clock::now();
    expect(rig.roster.note(button("/dev/input/event7", Button::A, true)), "a press is noted");
    expect(!rig.roster.note(button("/dev/input/event7", Button::A, true)),
           "a repeat changes nothing");
    expect(!rig.roster.note(button("", Button::B, true)), "an event without a pad is not a pad's");
    const auto list = rig.roster.controllers(now);
    expect(list.size() == 2 && list[0].player == 1 && list[1].player == 2 &&
               list[0].name == "Pad One",
           "player order");
    expect(list[0].held.empty() && list[1].held == std::vector<Button>{Button::A},
           "held buttons are the named pad's");
    expect(test::need(list[0].battery, "a battery").percent == 70 && !list[1].battery, "batteries");
    expect(rig.roster.note(button("/dev/input/event7", Button::A, false)), "a release is noted");
    expect(rig.roster.controllers(now)[1].held.empty(), "nothing held now");
}

void batteryIsReadOncePerLifetime() {
    Rig rig;
    const auto now = app::ControllerRoster::Clock::now();
    static_cast<void>(rig.roster.controllers(now));
    static_cast<void>(rig.roster.controllers(now + std::chrono::seconds{1}));
    expect(rig.batteryReads == 2, "one read per pad within the lifetime");
    static_cast<void>(rig.roster.controllers(now + app::ControllerRoster::batteryLife));
    expect(rig.batteryReads == 4, "read again after it");
}

void aPadThatLeavesIsForgotten() {
    Rig rig;
    const auto now = app::ControllerRoster::Clock::now();
    static_cast<void>(rig.roster.note(button("/dev/input/event7", Button::X, true)));
    static_cast<void>(rig.roster.controllers(now));
    rig.pads.pop_back();
    expect(rig.roster.controllers(now).size() == 1, "one pad left");
    rig.pads.push_back({"/dev/input/event7", "Pad Two", "bb"});
    expect(rig.roster.controllers(now)[1].held.empty(), "it holds nothing when it returns");
}

} // namespace

int main() {
    padsAreInConnectionOrderWithTheirHeldButtons();
    batteryIsReadOncePerLifetime();
    aPadThatLeavesIsForgotten();
    std::printf("controller_roster: all checks passed\n");
    return 0;
}
