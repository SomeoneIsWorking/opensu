// Pads against real devices: uinput pads stand in for controllers, and the test reads the virtual
// pad a game would read.
#include "pads.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "check.hpp"

namespace {

using namespace std::chrono_literals;
using opensu::gamepad::Button;
using opensu::gamepad::Chord;
using opensu::gamepad::ChordTriggers;
using opensu::gamepad::EvdevDevice;
using opensu::gamepad::Event;
using opensu::gamepad::OwnVirtualPads;
using opensu::gamepad::PadEvent;
using opensu::gamepad::PadInfo;
using opensu::gamepad::Pads;
using opensu::gamepad::VirtualPad;
using opensu::test::expect;
using opensu::test::fail;

/// A DualSense-shaped controller: 0..255 sticks, a hat, Guide, and a digital L2 as a Switch pad
/// has.
class FakePad {
  public:
    FakePad(const char* name, std::uint16_t vendor, std::uint16_t product) {
        fd_ = ::open("/dev/uinput", O_WRONLY | O_CLOEXEC);
        expect(fd_ >= 0, "/dev/uinput opens");
        ioctl(fd_, UI_SET_EVBIT, EV_KEY);
        ioctl(fd_, UI_SET_EVBIT, EV_ABS);
        for (const int key : {BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST, BTN_MODE, BTN_TL2}) {
            ioctl(fd_, UI_SET_KEYBIT, key);
        }
        for (const int axis : {ABS_X, ABS_Y, ABS_HAT0X, ABS_HAT0Y}) {
            ioctl(fd_, UI_SET_ABSBIT, axis);
            uinput_abs_setup setup{};
            setup.code = static_cast<std::uint16_t>(axis);
            const bool hat = axis == ABS_HAT0X || axis == ABS_HAT0Y;
            setup.absinfo.minimum = hat ? -1 : 0;
            setup.absinfo.maximum = hat ? 1 : 255;
            setup.absinfo.value = hat ? 0 : 128;
            ioctl(fd_, UI_ABS_SETUP, &setup);
        }
        uinput_setup setup{};
        setup.id.bustype = BUS_USB;
        setup.id.vendor = vendor;
        setup.id.product = product;
        std::snprintf(setup.name, sizeof(setup.name), "%s", name);
        expect(ioctl(fd_, UI_DEV_SETUP, &setup) == 0, "the fake pad is set up");
        expect(ioctl(fd_, UI_DEV_CREATE) == 0, "the fake pad is created");
    }
    FakePad(const FakePad&) = delete;
    FakePad& operator=(const FakePad&) = delete;
    ~FakePad() {
        if (fd_ >= 0) {
            ioctl(fd_, UI_DEV_DESTROY);
            ::close(fd_);
        }
    }

    void send(std::vector<PadEvent> events) const {
        events.push_back(PadEvent{EV_SYN, SYN_REPORT, 0});
        for (const PadEvent& event : events) {
            input_event out{};
            out.type = event.type;
            out.code = event.code;
            out.value = event.value;
            expect(::write(fd_, &out, sizeof(out)) == sizeof(out), "the fake pad takes input");
        }
    }

  private:
    int fd_{-1};
};

/// Collects the pads' events until `done` holds or two seconds pass.
bool collect(Pads& pads, std::vector<Event>& seen, const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        for (Event& event : pads.takeEvents()) {
            seen.push_back(std::move(event));
        }
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

bool has(const std::vector<Event>& seen, Event::Kind kind, const std::string& device) {
    return std::ranges::any_of(seen, [&](const Event& event) {
        return event.kind == kind && event.device == device;
    });
}

bool pressed(const std::vector<Event>& seen, Button button) {
    return std::ranges::any_of(seen, [button](const Event& event) {
        return event.kind == Event::Kind::Button && event.button == button && event.pressed;
    });
}

/// The node of the device named `name`, opened for reading whatever it is.
std::optional<EvdevDevice> openNamed(std::string_view name) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
        for (int n = 0; n < 512; ++n) {
            const std::string node = "/dev/input/event" + std::to_string(n);
            if (::access(node.c_str(), R_OK) != 0) {
                continue;
            }
            EvdevDevice device{node};
            if (device.name() == name) {
                return device;
            }
        }
        std::this_thread::sleep_for(20ms);
    }
    return std::nullopt;
}

/// Every node named `name` once at least one exists. A real pad plugged in while the test runs
/// is held too, so there can be more than one virtual pad.
std::vector<EvdevDevice> openAllNamed(std::string_view name) {
    std::vector<EvdevDevice> devices;
    if (!openNamed(name)) {
        return devices;
    }
    for (int n = 0; n < 512; ++n) {
        const std::string node = "/dev/input/event" + std::to_string(n);
        if (::access(node.c_str(), R_OK) != 0) {
            continue;
        }
        EvdevDevice device{node};
        if (device.name() == name) {
            devices.push_back(std::move(device));
        }
    }
    return devices;
}

/// Reads until `wanted` has been seen or a second passes, keeping everything seen.
bool readUntil(EvdevDevice& device, PadEvent wanted, std::vector<PadEvent>& seen) {
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (std::chrono::steady_clock::now() < deadline) {
        device.read(seen);
        if (std::ranges::find(seen, wanted) != seen.end()) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

} // namespace

int main() {
    if (::access("/dev/uinput", W_OK) != 0) {
        std::printf("pads: skipped, /dev/uinput is not writable\n");
        return 77;
    }
    Pads pads;
    std::vector<Event> seen;

    // Connected after the reader started, as a pad switched on mid-session is.
    std::optional<FakePad> physical{std::in_place, "opensu test pad", 0x054c, 0x0ce6};
    expect(collect(pads, seen,
                   [&] {
                       return has(seen, Event::Kind::Connected, "opensu test pad");
                   }),
           "a pad connected later is picked up");
    physical->send({{EV_KEY, BTN_SOUTH, 1}});
    expect(collect(pads, seen,
                   [&] {
                       return pressed(seen, Button::A);
                   }),
           "its presses are read");
    physical->send({{EV_KEY, BTN_SOUTH, 0}});

    std::string testId;
    for (const Event& event : seen) {
        if (event.kind == Event::Kind::Connected && event.device == "opensu test pad") {
            testId = event.source;
        }
    }
    expect(testId.starts_with("/dev/input/event"), "a connection names the pad's node");
    const auto listed = pads.connected();
    const auto mine = std::ranges::find(listed, testId, &PadInfo::id);
    expect(mine != listed.end() && mine->name == "opensu test pad", "connected() lists the pad");
    expect(std::ranges::any_of(seen,
                               [&](const Event& event) {
                                   return event.kind == Event::Kind::Button &&
                                          event.button == Button::A && event.source == testId;
                               }),
           "a button event names the pad that pressed it");
    {
        const FakePad second{"opensu second pad", 0x054c, 0x0ce6};
        expect(collect(pads, seen,
                       [&] {
                           return has(seen, Event::Kind::Connected, "opensu second pad");
                       }),
               "a second pad is picked up");
        const auto both = pads.connected();
        const auto first = std::ranges::find(both, testId, &PadInfo::id);
        const auto later =
            std::ranges::find(both, std::string{"opensu second pad"}, &PadInfo::name);
        expect(first != both.end() && later != both.end() && first < later,
               "pads are listed in the order they connected");
    }
    expect(collect(pads, seen,
                   [&] {
                       return has(seen, Event::Kind::Disconnected, "opensu second pad");
                   }),
           "the second pad leaves");
    expect(std::ranges::none_of(pads.connected(),
                                [](const PadInfo& info) {
                                    return info.name == "opensu second pad";
                                }),
           "a pad that left is no longer listed");

    {
        const FakePad steamVirtual{"Steam Virtual Gamepad", 0x28de, 0x11ff};
        auto node = openNamed("Steam Virtual Gamepad");
        if (!node) {
            fail("Steam's virtual pad appears");
        }
        std::this_thread::sleep_for(200ms);
        for (const Event& event : pads.takeEvents()) {
            expect(event.device != "Steam Virtual Gamepad", "Steam's virtual pad is not read");
        }
    }

    expect(!pads.hold().empty(), "the game is told to read only the virtual pads");
    std::vector<EvdevDevice> virtualPads = openAllNamed(VirtualPad::name);
    expect(!virtualPads.empty(), "holding gives the pad a virtual pad");
    auto other = openNamed("opensu test pad");
    if (!other) {
        fail("the held pad can still be opened");
    }

    std::vector<PadEvent> read;
    physical->send({{EV_KEY, BTN_SOUTH, 1}, {EV_ABS, ABS_X, 255}});
    // The test pad's virtual pad is the one its A reaches.
    std::optional<EvdevDevice> game;
    std::vector<std::vector<PadEvent>> candidateReads(virtualPads.size());
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (!game && std::chrono::steady_clock::now() < deadline) {
        for (std::size_t i = 0; i < virtualPads.size(); ++i) {
            virtualPads[i].read(candidateReads[i]);
            if (std::ranges::find(candidateReads[i], PadEvent{EV_KEY, BTN_SOUTH, 1}) !=
                candidateReads[i].end()) {
                read = std::move(candidateReads[i]);
                game = std::move(virtualPads[i]);
                break;
            }
        }
        std::this_thread::sleep_for(5ms);
    }
    if (!game) {
        fail("A reaches the game");
    }
    expect(readUntil(*game, {EV_ABS, ABS_X, 32767}, read), "the stick reaches it rescaled");
    std::vector<PadEvent> leaked;
    other->read(leaked);
    expect(leaked.empty(), "nothing else reads a held pad");

    {
        // A game process reading pads, as opensu-retro does: the virtual pad is its controller.
        Pads reader{"/dev/input", OwnVirtualPads::Read};
        std::vector<Event> heard;
        const std::string virtualName{VirtualPad::name};
        expect(collect(reader, heard,
                       [&] {
                           return has(heard, Event::Kind::Connected, virtualName);
                       }),
               "a game's reader takes the virtual pad");
        physical->send({{EV_KEY, BTN_WEST, 1}});
        expect(collect(reader, heard,
                       [&] {
                           return pressed(heard, Button::Y);
                       }),
               "it reads the held pad's presses through the virtual pad");
        const auto readerPads = reader.connected();
        for (const Event& event : heard) {
            if (event.kind == Event::Kind::Button) {
                const auto from = std::ranges::find(readerPads, event.source, &PadInfo::id);
                expect(from != readerPads.end() && from->name == virtualName,
                       "only the virtual pad moves while the real one is held");
            }
        }
        read.clear();
        expect(readUntil(*game, {EV_KEY, BTN_WEST, 1}, read),
               "reading takes nothing from the game");
        physical->send({{EV_KEY, BTN_WEST, 0}});
        expect(readUntil(*game, {EV_KEY, BTN_WEST, 0}, read), "the release reaches the game too");
    }

    seen.clear();
    physical->send({{EV_KEY, BTN_MODE, 1}});
    expect(collect(pads, seen,
                   [&] {
                       return pressed(seen, Button::Guide);
                   }),
           "Guide is reported to opensu");
    read.clear();
    game->read(read);
    for (const PadEvent& event : read) {
        expect(event.code != BTN_MODE || event.type != EV_KEY, "Guide never reaches the game");
    }

    pads.setBlocked(true);
    read.clear();
    expect(readUntil(*game, {EV_KEY, BTN_SOUTH, 0}, read), "blocking releases A");
    expect(readUntil(*game, {EV_ABS, ABS_X, 0}, read), "and centres the stick");
    seen.clear();
    physical->send({{EV_KEY, BTN_EAST, 1}});
    expect(collect(pads, seen,
                   [&] {
                       return pressed(seen, Button::B);
                   }),
           "opensu still sees B");
    read.clear();
    game->read(read);
    for (const PadEvent& event : read) {
        expect(event.type == EV_SYN, "nothing reaches the game while blocked");
    }

    // The resume is one write, so it is read whole.
    pads.setBlocked(false);
    expect(readUntil(*game, {EV_ABS, ABS_X, 32767}, read), "unblocking restores the stick");
    for (const PadEvent& event : read) {
        expect(event.type != EV_KEY, "B, pressed while blocked, stays up");
    }

    // Chords: Guide + A and L2 + X are bound, so none of their buttons reach the game.
    physical->send({{EV_KEY, BTN_SOUTH, 0}, {EV_KEY, BTN_EAST, 0}, {EV_KEY, BTN_MODE, 0}});
    pads.setChords(ChordTriggers{{Chord{Button::Guide, Button::A}, Chord{Button::L2, Button::X}},
                                 std::chrono::seconds{5}});
    physical->send({{EV_KEY, BTN_MODE, 1}, {EV_KEY, BTN_SOUTH, 1}});
    physical->send({{EV_KEY, BTN_SOUTH, 0}, {EV_KEY, BTN_MODE, 0}});
    physical->send({{EV_KEY, BTN_TL2, 1}, {EV_KEY, BTN_NORTH, 1}});
    physical->send({{EV_KEY, BTN_NORTH, 0}, {EV_KEY, BTN_TL2, 0}});
    physical->send({{EV_KEY, BTN_WEST, 1}});
    read.clear();
    expect(readUntil(*game, {EV_KEY, BTN_WEST, 1}, read), "a plain press after the chords arrives");
    for (const PadEvent& event : read) {
        expect(!(event.type == EV_KEY && event.code == BTN_SOUTH && event.value == 1),
               "Guide + A sends no A to the game");
        expect(!(event.type == EV_KEY && event.code == BTN_NORTH && event.value == 1),
               "L2 + X sends no X");
        expect(!(event.type == EV_ABS && event.code == ABS_Z && event.value != 0), "and no L2");
    }
    physical->send({{EV_KEY, BTN_WEST, 0}});
    physical->send({{EV_KEY, BTN_TL2, 1}});
    physical->send({{EV_KEY, BTN_TL2, 0}});
    read.clear();
    expect(readUntil(*game, {EV_ABS, ABS_Z, 255}, read) &&
               readUntil(*game, {EV_ABS, ABS_Z, 0}, read),
           "an L2 tap with no chord reaches the game on its release");
    pads.setChords({});

    pads.release();
    physical->send({{EV_KEY, BTN_NORTH, 1}});
    leaked.clear();
    expect(readUntil(*other, {EV_KEY, BTN_NORTH, 1}, leaked), "a released pad reaches others");

    seen.clear();
    physical.reset();
    expect(collect(pads, seen,
                   [&] {
                       return has(seen, Event::Kind::Disconnected, "opensu test pad");
                   }),
           "a pad that leaves is reported");
    expect(std::ranges::none_of(pads.connected(),
                                [&](const PadInfo& info) {
                                    return info.id == testId;
                                }),
           "and no longer listed");

    std::printf("pads: all checks passed\n");
    return 0;
}
