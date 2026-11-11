// A DolphinBar slot's remote through `WiiSlot`, against a stand-in remote on the far end of a
// packet socket pair, which keeps each report whole as a hidraw node does: an empty slot refuses
// the status request, a remote answers it and gets its player light and report mode, a Nunchuk
// and a Classic Controller are set up unencrypted and named, and their bytes come out as
// hid-wiimote's events, which `WiiRemoteLayout` reads as a pad.
#include "wii_slot.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/input-event-codes.h>

#include "check.hpp"
#include "fake_bar_remote.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::gamepad::PadEvent;
using opensu::gamepad::WiiPartEvent;
using opensu::gamepad::WiiRemoteLayout;
using opensu::gamepad::WiiRemotePart;
using opensu::gamepad::WiiSlot;
using opensu::test::expect;
using opensu::test::FakeBarRemote;

struct Pair {
    WiiSlot slot;
    int far;
};

Pair pair(int index) {
    std::array<int, 2> ends{};
    expect(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, ends.data()) ==
               0,
           "a socket pair stands in for the slot's node");
    return Pair{WiiSlot{ends[0], "/dev/hidraw-test", index}, ends[1]};
}

/// Lets the slot and the remote talk until neither has more to say.
std::vector<WiiPartEvent> talk(WiiSlot& slot, FakeBarRemote& remote) {
    std::vector<WiiPartEvent> events;
    for (int round = 0; round < 16; ++round) {
        expect(slot.read(events), "the slot stays");
        if (remote.serve() == 0) {
            expect(slot.read(events), "the slot stays");
            if (remote.serve() == 0) {
                break;
            }
        }
    }
    return events;
}

bool has(const std::vector<WiiPartEvent>& events, WiiRemotePart part, std::uint16_t type,
         std::uint16_t code, std::int32_t value) {
    return std::ranges::find(events, WiiPartEvent{part, PadEvent{type, code, value}}) !=
           events.end();
}

void testEmpty() {
    Pair empty = pair(0);
    // An empty slot refuses the write.
    ::shutdown(empty.far, SHUT_RD);
    expect(!empty.slot.ask() && !empty.slot.remote(), "an empty slot refuses the status request");
    ::close(empty.far);
}

void testRemote() {
    Pair bar = pair(2);
    FakeBarRemote remote{bar.far};
    expect(bar.slot.ask(), "a slot with a remote takes the status request");
    talk(bar.slot, remote);
    expect(bar.slot.remote() && !bar.slot.extension(), "its status says a remote is there");
    expect(remote.lights() == 0x40, "the third slot lights the third player light");
    expect(remote.mode() == 0x30, "and asks for the buttons");

    remote.report(0x08, 0x01);
    std::vector<WiiPartEvent> events = talk(bar.slot, remote);
    expect(has(events, WiiRemotePart::Remote, EV_KEY, KEY_UP, 1) &&
               has(events, WiiRemotePart::Remote, EV_KEY, BTN_2, 1) &&
               has(events, WiiRemotePart::Remote, EV_SYN, SYN_REPORT, 0),
           "up and 2 come as hid-wiimote's KEY_UP and BTN_2");
    remote.report(0x00, 0x01);
    events = talk(bar.slot, remote);
    expect(events.size() == 2 && has(events, WiiRemotePart::Remote, EV_KEY, KEY_UP, 0),
           "only what changed is said");

    remote.plug(FakeBarRemote::Extension::Nunchuk);
    talk(bar.slot, remote);
    expect(remote.unencrypted(), "the extension is set up unencrypted");
    expect(bar.slot.extension() == WiiRemotePart::Nunchuk, "and named a Nunchuk by its id");
    expect(remote.mode() == 0x32, "the buttons come with the extension's bytes");
    remote.report(0x00, 0x00, {0x80 + 100, 0x80, 0x80, 0x80, 0x80, 0xfe});
    events = talk(bar.slot, remote);
    expect(has(events, WiiRemotePart::Nunchuk, EV_ABS, ABS_HAT0X, 100) &&
               has(events, WiiRemotePart::Nunchuk, EV_KEY, BTN_Z, 1),
           "the stick right and Z come as the Nunchuk's");
    const WiiRemoteLayout upright{WiiRemotePart::Nunchuk,
                                  WiiSlot::capabilitiesOf(WiiRemotePart::Nunchuk)};
    expect(upright.map(WiiRemotePart::Nunchuk, {EV_KEY, BTN_Z, 1}) == PadEvent{EV_KEY, BTN_TL2, 1},
           "which the layout reads as a Bluetooth remote's");

    remote.plug(FakeBarRemote::Extension::None);
    talk(bar.slot, remote);
    expect(!bar.slot.extension() && remote.mode() == 0x30, "pulled out, it is gone");
    ::close(bar.far);
}

void testClassic() {
    Pair bar = pair(0);
    FakeBarRemote remote{bar.far};
    remote.plug(FakeBarRemote::Extension::Classic);
    talk(bar.slot, remote);
    expect(bar.slot.remote() && bar.slot.extension() == WiiRemotePart::Classic,
           "a remote that arrives with its Classic Controller has it named");
    std::array<std::uint8_t, 6> bytes = FakeBarRemote::classicIdle();
    bytes[5] = static_cast<std::uint8_t>(bytes[5] & ~0x40);          // B
    bytes[0] = static_cast<std::uint8_t>((bytes[0] & ~0x3f) | 0x3f); // left stick fully right
    remote.report(0x00, 0x00, bytes);
    const std::vector<WiiPartEvent> events = talk(bar.slot, remote);
    expect(has(events, WiiRemotePart::Classic, EV_KEY, BTN_B, 1), "its B comes as BTN_B");
    expect(has(events, WiiRemotePart::Classic, EV_ABS, ABS_HAT1X, 31),
           "its left stick on ABS_HAT1X");
    expect(std::ranges::none_of(events,
                                [](const WiiPartEvent& event) {
                                    return event.event.type == EV_KEY && event.event.value == 1 &&
                                           event.event.code != BTN_B;
                                }),
           "and nothing else is held");
    const std::vector<WiiPartEvent> state = bar.slot.state();
    expect(has(state, WiiRemotePart::Classic, EV_KEY, BTN_B, 1), "its state holds B");
    ::close(bar.far);
}

} // namespace

int main() try {
    // The stand-in's end closing must not end the test.
    std::signal(SIGPIPE, SIG_IGN);
    testEmpty();
    testRemote();
    testClassic();
    std::printf("wii_slot: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
