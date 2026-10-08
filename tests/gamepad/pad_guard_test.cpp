// The guard against real devices: a uinput pad stands in for a controller, and the test reads the
// virtual pad a game would read.
#include "pad_guard.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
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
using iideck::gamepad::Button;
using iideck::gamepad::EvdevDevice;
using iideck::gamepad::Event;
using iideck::gamepad::PadEvent;
using iideck::gamepad::PadGuard;
using iideck::gamepad::VirtualPad;
using iideck::test::expect;

constexpr const char* fakePhys = "iideck/test-pad";

/// A DualSense-shaped controller: 0..255 sticks, a hat, Guide.
class FakePad {
  public:
    FakePad() {
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
        ioctl(fd_, UI_SET_PHYS, fakePhys);
        uinput_setup setup{};
        setup.id.bustype = BUS_USB;
        setup.id.vendor = 0x054c;
        setup.id.product = 0x0ce6;
        std::snprintf(setup.name, sizeof(setup.name), "iideck test pad");
        expect(ioctl(fd_, UI_DEV_SETUP, &setup) == 0, "the fake pad is set up");
        expect(ioctl(fd_, UI_DEV_CREATE) == 0, "the fake pad is created");
    }
    FakePad(const FakePad&) = delete;
    FakePad& operator=(const FakePad&) = delete;
    ~FakePad() {
        ioctl(fd_, UI_DEV_DESTROY);
        ::close(fd_);
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

/// The node whose phys matches, once udev has made it readable.
std::optional<EvdevDevice> waitFor(const std::string& phys) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        for (auto& device : EvdevDevice::openGamepads("/dev/input")) {
            if (device.phys() == phys) {
                return std::move(device);
            }
        }
        std::this_thread::sleep_for(20ms);
    }
    return std::nullopt;
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

/// Waits for the guard to report a press. The guard forwards a batch before reporting its
/// controls, so whatever the batch sent the game is readable by then.
bool reported(PadGuard& guard, Button button) {
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (std::chrono::steady_clock::now() < deadline) {
        for (const Event& event : guard.takeControls()) {
            if (event.button == button && event.pressed) {
                return true;
            }
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

/// The virtual pad's own node, which openGamepads skips.
std::optional<EvdevDevice> waitForVirtual() {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        for (int n = 0; n < 512; ++n) {
            const std::string node = "/dev/input/event" + std::to_string(n);
            if (::access(node.c_str(), R_OK) != 0) {
                continue;
            }
            EvdevDevice device{node};
            if (device.phys() == VirtualPad::phys) {
                return device;
            }
        }
        std::this_thread::sleep_for(20ms);
    }
    return std::nullopt;
}

} // namespace

int main() {
    if (::access("/dev/uinput", W_OK) != 0) {
        std::printf("pad_guard: skipped, /dev/uinput is not writable\n");
        return 77;
    }
    const FakePad physical;
    {
        auto ready = waitFor(fakePhys);
        expect(ready.has_value(), "the fake pad appears as a gamepad");
    }

    PadGuard guard;
    expect(guard.held() >= 1, "the guard holds the fake pad");
    expect(!guard.environment().empty(), "the game is told to read only the virtual pad");
    auto game = waitForVirtual();
    expect(game.has_value(), "the virtual pad appears");
    auto other = waitFor(fakePhys);
    expect(other.has_value(), "the grabbed pad can still be opened");

    std::vector<PadEvent> seen;
    physical.send({{EV_KEY, BTN_SOUTH, 1}, {EV_ABS, ABS_X, 255}});
    expect(readUntil(*game, {EV_KEY, BTN_SOUTH, 1}, seen), "A reaches the game");
    expect(readUntil(*game, {EV_ABS, ABS_X, 32767}, seen), "the stick reaches it rescaled");
    std::vector<PadEvent> leaked;
    other->read(leaked);
    expect(leaked.empty(), "nothing else reads the grabbed pad");

    physical.send({{EV_KEY, BTN_MODE, 1}});
    expect(reported(guard, Button::Guide), "Guide is reported to iideck");
    seen.clear();
    game->read(seen);
    for (const PadEvent& event : seen) {
        expect(event.code != BTN_MODE || event.type != EV_KEY, "Guide never reaches the game");
    }

    guard.setBlocked(true);
    seen.clear();
    expect(readUntil(*game, {EV_KEY, BTN_SOUTH, 0}, seen), "blocking releases A");
    expect(readUntil(*game, {EV_ABS, ABS_X, 0}, seen), "and centres the stick");
    physical.send({{EV_KEY, BTN_EAST, 1}});
    expect(reported(guard, Button::B), "iideck still sees B");
    seen.clear();
    game->read(seen);
    for (const PadEvent& event : seen) {
        expect(event.type == EV_SYN, "nothing reaches the game while blocked");
    }

    // The resume is one write, so it is read whole.
    guard.setBlocked(false);
    expect(readUntil(*game, {EV_ABS, ABS_X, 32767}, seen), "unblocking restores the stick");
    for (const PadEvent& event : seen) {
        expect(event.type != EV_KEY, "B, pressed while blocked, stays up");
    }

    std::printf("pad_guard: all checks passed\n");
    return 0;
}
