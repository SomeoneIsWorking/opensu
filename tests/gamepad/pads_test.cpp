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
using opensu::gamepad::EvdevDevice;
using opensu::gamepad::Event;
using opensu::gamepad::PadEvent;
using opensu::gamepad::Pads;
using opensu::gamepad::VirtualPad;
using opensu::test::expect;
using opensu::test::fail;

/// A DualSense-shaped controller: 0..255 sticks, a hat, Guide.
class FakePad {
  public:
    FakePad(const char* name, std::uint16_t vendor, std::uint16_t product) {
        fd_ = ::open("/dev/uinput", O_WRONLY | O_CLOEXEC);
        expect(fd_ >= 0, "/dev/uinput opens");
        ioctl(fd_, UI_SET_EVBIT, EV_KEY);
        ioctl(fd_, UI_SET_EVBIT, EV_ABS);
        for (const int key : {BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST, BTN_MODE}) {
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

    std::printf("pads: all checks passed\n");
    return 0;
}
