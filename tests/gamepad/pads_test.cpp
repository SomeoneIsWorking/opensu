// EvdevPads against real devices: uinput pads stand in for controllers, and the test reads the
// virtual pad a game would read. Every device is found by the uinput descriptor that made it, never
// by name, and every EvdevPads reads a private directory linking only the test's own nodes, so a
// second run or any other controller on the machine is never opened, read or grabbed. The fake
// pads carry openSU's virtual pad mark, so a running openSU leaves them alone. A DolphinBar's slot
// is a pseudo-terminal under a private bar tree, a stand-in remote on its far end.
#include "evdev_pads.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <cstdlib>
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "check.hpp"
#include "fake_bar_remote.hpp"
#include "support/test_main.hpp"

namespace {

using namespace std::chrono_literals;
using opensu::gamepad::Button;
using opensu::gamepad::Chord;
using opensu::gamepad::ChordTriggers;
using opensu::gamepad::Claim;
using opensu::gamepad::EvdevDevice;
using opensu::gamepad::EvdevPads;
using opensu::gamepad::Event;
using opensu::gamepad::ForwardMap;
using opensu::gamepad::HeldPads;
using opensu::gamepad::OwnVirtualPads;
using opensu::gamepad::PadEvent;
using opensu::gamepad::PadInfo;
using opensu::gamepad::VirtualPad;
using opensu::gamepad::WiiRemoteHold;
using opensu::test::expect;
using opensu::test::fail;
using opensu::test::FakeBarRemote;
namespace fs = std::filesystem;

/// The event node of the uinput device made through `fd`, once it is open to this user. No node
/// is opened to find it.
fs::path nodeOf(int fd) {
    std::array<char, 64> sysname{};
    expect(ioctl(fd, UI_GET_SYSNAME(sysname.size()), sysname.data()) >= 0,
           "uinput names its device");
    const fs::path device = fs::path{"/sys/devices/virtual/input"} / sysname.data();
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
        std::error_code error;
        for (const auto& entry : fs::directory_iterator{device, error}) {
            const std::string leaf = entry.path().filename().string();
            if (leaf.starts_with("event") && ::access(("/dev/input/" + leaf).c_str(), R_OK) == 0) {
                return "/dev/input/" + leaf;
            }
        }
        std::this_thread::sleep_for(20ms);
    }
    fail("the test's node appears");
}

/// The nodes of every uinput device this process holds open, except `besides`: the virtual pads
/// its EvdevPads made, never another process's.
std::vector<fs::path> ownUinputNodes(const std::vector<fs::path>& besides) {
    std::vector<fs::path> nodes;
    for (const auto& entry : fs::directory_iterator{"/proc/self/fd"}) {
        std::error_code error;
        if (fs::read_symlink(entry.path(), error) != "/dev/uinput") {
            continue;
        }
        const fs::path node = nodeOf(std::stoi(entry.path().filename().string()));
        if (std::ranges::find(besides, node) == besides.end()) {
            nodes.push_back(node);
        }
    }
    std::ranges::sort(nodes);
    return nodes;
}

std::vector<EvdevDevice> openAll(const std::vector<fs::path>& nodes) {
    std::vector<EvdevDevice> devices;
    devices.reserve(nodes.size());
    for (const fs::path& node : nodes) {
        devices.emplace_back(node);
    }
    return devices;
}

/// A directory of links to the nodes an EvdevPads may read.
class InputDir {
  public:
    explicit InputDir(std::string_view tag)
        : path_{fs::path{OPENSU_TEST_SCRATCH} /
                ("pads-" + std::string{tag} + "-" + std::to_string(::getpid()))} {
        fs::remove_all(path_);
        fs::create_directories(path_);
    }
    InputDir(const InputDir&) = delete;
    InputDir& operator=(const InputDir&) = delete;
    ~InputDir() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    [[nodiscard]] const fs::path& path() const {
        return path_;
    }
    /// Links `node` in, as udev would add it; returns the link.
    fs::path add(const fs::path& node) const {
        const fs::path link = path_ / node.filename();
        fs::remove(link);
        fs::create_symlink(node, link);
        return link;
    }

  private:
    fs::path path_;
};

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
        ioctl(fd_, UI_SET_PHYS, std::string{VirtualPad::phys}.c_str());
        uinput_setup setup{};
        setup.id.bustype = BUS_USB;
        setup.id.vendor = vendor;
        setup.id.product = product;
        std::snprintf(setup.name, sizeof(setup.name), "%s", name);
        expect(ioctl(fd_, UI_DEV_SETUP, &setup) == 0, "the fake pad is set up");
        expect(ioctl(fd_, UI_DEV_CREATE) == 0, "the fake pad is created");
        node_ = nodeOf(fd_);
    }
    FakePad(const FakePad&) = delete;
    FakePad& operator=(const FakePad&) = delete;
    ~FakePad() {
        if (fd_ >= 0) {
            ioctl(fd_, UI_DEV_DESTROY);
            ::close(fd_);
        }
    }

    [[nodiscard]] const fs::path& node() const {
        return node_;
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
    fs::path node_;
};

/// A Bluetooth Wii Remote's buttons as hid-wiimote makes them: keys only, Nintendo's ids.
class FakeWiiRemote {
  public:
    FakeWiiRemote() {
        fd_ = ::open("/dev/uinput", O_WRONLY | O_CLOEXEC);
        expect(fd_ >= 0, "/dev/uinput opens");
        ioctl(fd_, UI_SET_EVBIT, EV_KEY);
        for (const int key : {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_NEXT, KEY_PREVIOUS, BTN_1,
                              BTN_2, BTN_A, BTN_B, BTN_MODE}) {
            ioctl(fd_, UI_SET_KEYBIT, key);
        }
        ioctl(fd_, UI_SET_PHYS, std::string{VirtualPad::phys}.c_str());
        uinput_setup setup{};
        setup.id.bustype = BUS_BLUETOOTH;
        setup.id.vendor = 0x057e;
        setup.id.product = 0x0306;
        std::snprintf(setup.name, sizeof(setup.name), "%s", "Nintendo Wii Remote");
        expect(ioctl(fd_, UI_DEV_SETUP, &setup) == 0, "the fake remote is set up");
        expect(ioctl(fd_, UI_DEV_CREATE) == 0, "the fake remote is created");
        node_ = nodeOf(fd_);
    }
    FakeWiiRemote(const FakeWiiRemote&) = delete;
    FakeWiiRemote& operator=(const FakeWiiRemote&) = delete;
    ~FakeWiiRemote() {
        ioctl(fd_, UI_DEV_DESTROY);
        ::close(fd_);
    }

    [[nodiscard]] const fs::path& node() const {
        return node_;
    }

    void press(std::uint16_t key, std::int32_t value) const {
        for (const input_event out :
             {input_event{{}, EV_KEY, key, value}, input_event{{}, EV_SYN, SYN_REPORT, 0}}) {
            expect(::write(fd_, &out, sizeof(out)) == sizeof(out), "the fake remote takes input");
        }
    }

  private:
    int fd_{-1};
    fs::path node_;
};

/// A Wii Remote's IR camera as hid-wiimote makes it: four dots, Nintendo's ids; it shares the fake
/// remote's phys, as a real camera shares its remote's parent device.
class FakeWiiCamera {
  public:
    FakeWiiCamera() {
        fd_ = ::open("/dev/uinput", O_WRONLY | O_CLOEXEC);
        expect(fd_ >= 0, "/dev/uinput opens");
        ioctl(fd_, UI_SET_EVBIT, EV_ABS);
        for (int axis = ABS_HAT0X; axis <= ABS_HAT3Y; ++axis) {
            ioctl(fd_, UI_SET_ABSBIT, axis);
            uinput_abs_setup setup{};
            setup.code = static_cast<std::uint16_t>(axis);
            setup.absinfo.minimum = 0;
            setup.absinfo.maximum = (axis - ABS_HAT0X) % 2 == 0 ? 1023 : 767;
            setup.absinfo.value = 1023;
            ioctl(fd_, UI_ABS_SETUP, &setup);
        }
        ioctl(fd_, UI_SET_PHYS, std::string{VirtualPad::phys}.c_str());
        uinput_setup setup{};
        setup.id.bustype = BUS_BLUETOOTH;
        setup.id.vendor = 0x057e;
        setup.id.product = 0x0306;
        std::snprintf(setup.name, sizeof(setup.name), "%s", "Nintendo Wii Remote IR");
        expect(ioctl(fd_, UI_DEV_SETUP, &setup) == 0, "the fake camera is set up");
        expect(ioctl(fd_, UI_DEV_CREATE) == 0, "the fake camera is created");
        node_ = nodeOf(fd_);
    }
    FakeWiiCamera(const FakeWiiCamera&) = delete;
    FakeWiiCamera& operator=(const FakeWiiCamera&) = delete;
    ~FakeWiiCamera() {
        ioctl(fd_, UI_DEV_DESTROY);
        ::close(fd_);
    }

    [[nodiscard]] const fs::path& node() const {
        return node_;
    }

    /// The sensor bar's two ends seen at `left` and `right` across, `down` down.
    void see(std::int32_t left, std::int32_t right, std::int32_t down) const {
        for (const input_event out :
             {input_event{{}, EV_ABS, ABS_HAT0X, left}, input_event{{}, EV_ABS, ABS_HAT0Y, down},
              input_event{{}, EV_ABS, ABS_HAT1X, right}, input_event{{}, EV_ABS, ABS_HAT1Y, down},
              input_event{{}, EV_SYN, SYN_REPORT, 0}}) {
            expect(::write(fd_, &out, sizeof(out)) == sizeof(out), "the fake camera takes input");
        }
    }

  private:
    int fd_{-1};
    fs::path node_;
};

/// A Classic Controller plugged into the fake remote, as hid-wiimote makes its device: its buttons
/// by their labels, both sticks and the triggers' travel at +-30; it shares the remote's phys.
class FakeWiiClassic {
  public:
    FakeWiiClassic() {
        fd_ = ::open("/dev/uinput", O_WRONLY | O_CLOEXEC);
        expect(fd_ >= 0, "/dev/uinput opens");
        ioctl(fd_, UI_SET_EVBIT, EV_KEY);
        for (const int key :
             {BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, KEY_NEXT, KEY_PREVIOUS,
              BTN_MODE, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT}) {
            ioctl(fd_, UI_SET_KEYBIT, key);
        }
        ioctl(fd_, UI_SET_EVBIT, EV_ABS);
        for (int axis = ABS_HAT1X; axis <= ABS_HAT3Y; ++axis) {
            ioctl(fd_, UI_SET_ABSBIT, axis);
            uinput_abs_setup setup{};
            setup.code = static_cast<std::uint16_t>(axis);
            setup.absinfo.minimum = -30;
            setup.absinfo.maximum = 30;
            ioctl(fd_, UI_ABS_SETUP, &setup);
        }
        ioctl(fd_, UI_SET_PHYS, std::string{VirtualPad::phys}.c_str());
        uinput_setup setup{};
        setup.id.bustype = BUS_BLUETOOTH;
        setup.id.vendor = 0x057e;
        setup.id.product = 0x0306;
        std::snprintf(setup.name, sizeof(setup.name), "%s",
                      "Nintendo Wii Remote Classic Controller");
        expect(ioctl(fd_, UI_DEV_SETUP, &setup) == 0, "the fake Classic Controller is set up");
        expect(ioctl(fd_, UI_DEV_CREATE) == 0, "the fake Classic Controller is created");
        node_ = nodeOf(fd_);
    }
    FakeWiiClassic(const FakeWiiClassic&) = delete;
    FakeWiiClassic& operator=(const FakeWiiClassic&) = delete;
    ~FakeWiiClassic() {
        ioctl(fd_, UI_DEV_DESTROY);
        ::close(fd_);
    }

    [[nodiscard]] const fs::path& node() const {
        return node_;
    }

    void press(std::uint16_t key, std::int32_t value) const {
        for (const input_event out :
             {input_event{{}, EV_KEY, key, value}, input_event{{}, EV_SYN, SYN_REPORT, 0}}) {
            expect(::write(fd_, &out, sizeof(out)) == sizeof(out), "the fake Classic takes input");
        }
    }

  private:
    int fd_{-1};
    fs::path node_;
};

/// A DolphinBar in Wii Remote mode with one slot, as `device::HidPlaces` shows it: the slot's HID
/// folder and, for its hidraw node, a raw pseudo-terminal whose far end the test holds.
class FakeBar {
  public:
    explicit FakeBar(std::string_view tag)
        : root_{fs::path{OPENSU_TEST_SCRATCH} /
                ("bar-" + std::string{tag} + "-" + std::to_string(::getpid()))} {
        fs::remove_all(root_);
        const fs::path slot = root_ / "devices" / "0003:057E:0306.0001";
        fs::create_directories(slot / "hidraw" / "hidraw0");
        fs::create_directories(root_ / "dev");
        std::FILE* uevent = std::fopen((slot / "uevent").c_str(), "w");
        expect(uevent != nullptr, "the bar's uevent is written");
        std::fputs("DRIVER=hid-generic\nHID_ID=0003:0000057E:00000306\n"
                   "HID_PHYS=usb-test-1/input0\n",
                   uevent);
        std::fclose(uevent);
        far_ = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
        expect(far_ >= 0 && ::grantpt(far_) == 0 && ::unlockpt(far_) == 0,
               "a pseudo-terminal stands in for the slot");
        node_ = ::ptsname(far_);
        // Held open, so the far end stays while openSU closes the slot.
        near_ = ::open(node_.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);
        termios raw{};
        expect(near_ >= 0 && ::tcgetattr(near_, &raw) == 0, "the slot's end opens");
        ::cfmakeraw(&raw);
        expect(::tcsetattr(near_, TCSANOW, &raw) == 0, "and carries bytes as they are");
        fs::create_symlink(node_, root_ / "dev" / "hidraw0");
    }
    FakeBar(const FakeBar&) = delete;
    FakeBar& operator=(const FakeBar&) = delete;
    ~FakeBar() {
        ::close(near_);
        ::close(far_);
        std::error_code ignored;
        fs::remove_all(root_, ignored);
    }

    [[nodiscard]] opensu::device::HidPlaces places() const {
        return {.devices = root_ / "devices", .dev = root_ / "dev"};
    }
    /// The slot's node, as the pads name its remote.
    [[nodiscard]] fs::path slotNode() const {
        return root_ / "dev" / "hidraw0";
    }
    /// The terminal behind it.
    [[nodiscard]] const fs::path& terminal() const {
        return node_;
    }
    [[nodiscard]] int far() const {
        return far_;
    }

  private:
    fs::path root_;
    fs::path node_;
    int far_{-1};
    int near_{-1};
};

/// How many times this process holds `node` open (through any link to it).
std::size_t opened(const fs::path& node) {
    std::error_code error;
    const fs::path real = fs::canonical(node, error);
    std::size_t count = 0;
    for (const auto& entry : fs::directory_iterator{"/proc/self/fd"}) {
        if (fs::read_symlink(entry.path(), error) == real) {
            ++count;
        }
    }
    return count;
}

/// A pad's motion sensor node as hid-playstation and hid-nintendo make it: accelerometer and gyro
/// axes, the accelerometer property, no buttons.
class FakeMotion {
  public:
    explicit FakeMotion(const char* name) {
        fd_ = ::open("/dev/uinput", O_WRONLY | O_CLOEXEC);
        expect(fd_ >= 0, "/dev/uinput opens");
        ioctl(fd_, UI_SET_EVBIT, EV_ABS);
        ioctl(fd_, UI_SET_PROPBIT, INPUT_PROP_ACCELEROMETER);
        for (const int axis : {ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ}) {
            ioctl(fd_, UI_SET_ABSBIT, axis);
            uinput_abs_setup setup{};
            setup.code = static_cast<std::uint16_t>(axis);
            setup.absinfo.minimum = -32768;
            setup.absinfo.maximum = 32767;
            ioctl(fd_, UI_ABS_SETUP, &setup);
        }
        ioctl(fd_, UI_SET_PHYS, std::string{VirtualPad::phys}.c_str());
        uinput_setup setup{};
        setup.id.bustype = BUS_USB;
        setup.id.vendor = 0x054c;
        setup.id.product = 0x0ce6;
        std::snprintf(setup.name, sizeof(setup.name), "%s", name);
        expect(ioctl(fd_, UI_DEV_SETUP, &setup) == 0, "the fake motion node is set up");
        expect(ioctl(fd_, UI_DEV_CREATE) == 0, "the fake motion node is created");
        node_ = nodeOf(fd_);
    }
    FakeMotion(const FakeMotion&) = delete;
    FakeMotion& operator=(const FakeMotion&) = delete;
    ~FakeMotion() {
        ioctl(fd_, UI_DEV_DESTROY);
        ::close(fd_);
    }

    [[nodiscard]] const fs::path& node() const {
        return node_;
    }

    void tilt(std::int32_t value) const {
        for (const input_event out :
             {input_event{{}, EV_ABS, ABS_RX, value}, input_event{{}, EV_SYN, SYN_REPORT, 0}}) {
            expect(::write(fd_, &out, sizeof(out)) == sizeof(out), "the motion node takes input");
        }
    }

  private:
    int fd_{-1};
    fs::path node_;
};

/// Collects the pads' events until `done` holds or two seconds pass.
bool collect(EvdevPads& pads, std::vector<Event>& seen, const std::function<bool()>& done) {
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

int main() try {
    if (::access("/dev/uinput", W_OK) != 0) {
        std::printf("pads: skipped, /dev/uinput is not writable\n");
        return 77;
    }
    const InputDir inputs{"shell"};
    // Reads the fakes, which carry the virtual pad mark; the directory holds no virtual pad.
    EvdevPads pads{inputs.path(), std::nullopt, OwnVirtualPads::Read};
    std::vector<Event> seen;

    // Connected after the reader started, as a pad switched on mid-session is.
    std::optional<FakePad> physical{std::in_place, "opensu test pad", 0x054c, 0x0ce6};
    const fs::path testLink = inputs.add(physical->node());
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
    expect(testId == testLink.string(), "a connection names the pad's node");
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
        inputs.add(second.node());
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
        inputs.add(steamVirtual.node());
        std::this_thread::sleep_for(200ms);
        for (const Event& event : pads.takeEvents()) {
            expect(event.device != "Steam Virtual Gamepad", "Steam's virtual pad is not read");
        }
    }

    // The test pad's motion node, made before the hold as a pad brings it up with its gamepad node.
    const FakeMotion motion{"opensu test pad Motion Sensors"};
    inputs.add(motion.node());
    std::optional<EvdevDevice> motionNode{std::in_place, motion.node()};
    const std::vector<fs::path> fakes{physical->node(), motion.node()};
    const HeldPads held = pads.hold({}, WiiRemoteHold::AsPads);
    expect(!held.environment.empty(), "the game is told to read only the virtual pads");
    expect(held.padGuid == VirtualPad::sdlGuid(), "the game is told the virtual pads' GUID");
    std::vector<EvdevDevice> virtualPads = openAll(ownUinputNodes(fakes));
    expect(!virtualPads.empty(), "holding gives the pad a virtual pad");
    std::optional<EvdevDevice> other{std::in_place, physical->node()};

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
    expect(std::ranges::none_of(pads.connected(),
                                [](const PadInfo& info) {
                                    return info.name == "opensu test pad Motion Sensors";
                                }),
           "a motion node is no pad");
    // EVIOCGRAB fails with EBUSY while another reader holds the grab.
    expect(motionNode->grab(), "holding the pads leaves the motion node ungrabbed");
    motionNode->ungrab();
    std::vector<PadEvent> tilted;
    motion.tilt(1200);
    expect(readUntil(*motionNode, {EV_ABS, ABS_RX, 1200}, tilted),
           "and its motion reaches a game's reader while the pads are held");

    {
        // A game process reading pads, as opensu-retro does: the virtual pad is its controller.
        const InputDir gameInputs{"game"};
        for (const fs::path& node : ownUinputNodes(fakes)) {
            fs::create_symlink(node, gameInputs.path() / node.filename());
        }
        EvdevPads reader{gameInputs.path(), std::nullopt, OwnVirtualPads::Read};
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

    // Another window in front of the game: the pad is everyone's, the game's pad rests.
    pads.claim(Claim::Away);
    read.clear();
    expect(readUntil(*game, {EV_ABS, ABS_X, 0}, read), "away, the game's stick centres");
    seen.clear();
    leaked.clear();
    physical->send({{EV_KEY, BTN_NORTH, 1}});
    expect(readUntil(*other, {EV_KEY, BTN_NORTH, 1}, leaked), "the focused window reads the pad");
    expect(collect(pads, seen,
                   [&] {
                       return pressed(seen, Button::X);
                   }),
           "and opensu still hears it");
    read.clear();
    game->read(read);
    expect(std::ranges::find(read, PadEvent{EV_KEY, BTN_NORTH, 1}) == read.end(),
           "the game does not");
    physical->send({{EV_KEY, BTN_NORTH, 0}});
    expect(readUntil(*other, {EV_KEY, BTN_NORTH, 0}, leaked), "the release reaches it too");
    pads.claim(Claim::Game);
    read.clear();
    expect(readUntil(*game, {EV_ABS, ABS_X, 32767}, read), "back in front, the game has the stick");
    physical->send({{EV_KEY, BTN_NORTH, 1}});
    expect(readUntil(*game, {EV_KEY, BTN_NORTH, 1}, read), "and the presses");
    leaked.clear();
    other->read(leaked);
    expect(leaked.empty(), "which nothing else reads");
    physical->send({{EV_KEY, BTN_NORTH, 0}});

    pads.release(Claim::Free);
    physical->send({{EV_KEY, BTN_NORTH, 1}});
    leaked.clear();
    expect(readUntil(*other, {EV_KEY, BTN_NORTH, 1}, leaked), "a released pad reaches others");
    physical->send({{EV_KEY, BTN_NORTH, 0}});
    expect(other->grab(), "a free pad is no one's");
    other->ungrab();

    // The shell's claim: opensu reads the pad and nothing else does, Guide above all.
    pads.claim(Claim::Shell);
    expect(!other->grab(), "a shell-held pad cannot be grabbed by another reader");
    leaked.clear();
    other->read(leaked);
    seen.clear();
    physical->send({{EV_KEY, BTN_MODE, 1}});
    expect(collect(pads, seen,
                   [&] {
                       return pressed(seen, Button::Guide);
                   }),
           "the shell hears Guide");
    physical->send({{EV_KEY, BTN_MODE, 0}});
    std::this_thread::sleep_for(50ms);
    leaked.clear();
    other->read(leaked);
    expect(leaked.empty(), "and nothing else does");
    pads.claim(Claim::Free);
    physical->send({{EV_KEY, BTN_MODE, 1}});
    expect(readUntil(*other, {EV_KEY, BTN_MODE, 1}, leaked), "a freed pad reaches others again");
    physical->send({{EV_KEY, BTN_MODE, 0}});

    // The game's mapping: A reaches the virtual pad as B, the shell still hears A.
    expect(!pads.hold(ForwardMap{{{Button::A, {Button::B}}}, {}}, WiiRemoteHold::AsPads)
                .environment.empty(),
           "the pads are held with a mapping");
    std::vector<EvdevDevice> mappedPads = openAll(ownUinputNodes(fakes));
    expect(!mappedPads.empty(), "the mapped hold gives the pad a virtual pad");
    seen.clear();
    physical->send({{EV_KEY, BTN_SOUTH, 1}});
    std::vector<std::vector<PadEvent>> mappedReads(mappedPads.size());
    bool remapped = false;
    const auto mappedDeadline = std::chrono::steady_clock::now() + 1s;
    while (!remapped && std::chrono::steady_clock::now() < mappedDeadline) {
        for (std::size_t i = 0; i < mappedPads.size(); ++i) {
            mappedPads[i].read(mappedReads[i]);
            remapped =
                remapped || std::ranges::find(mappedReads[i], PadEvent{EV_KEY, BTN_EAST, 1}) !=
                                mappedReads[i].end();
        }
        std::this_thread::sleep_for(5ms);
    }
    expect(remapped, "a remapped A reaches the virtual pad as B");
    for (const auto& reads : mappedReads) {
        expect(std::ranges::find(reads, PadEvent{EV_KEY, BTN_SOUTH, 1}) == reads.end(),
               "and never as A");
    }
    expect(collect(pads, seen,
                   [&] {
                       return pressed(seen, Button::A);
                   }),
           "while opensu hears A");
    physical->send({{EV_KEY, BTN_SOUTH, 0}});
    pads.release(Claim::Free);

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

    {
        // A Bluetooth Wii Remote alone is a pad held sideways: 2 is A, the d-pad's up is Left.
        const FakeWiiRemote remote;
        inputs.add(remote.node());
        seen.clear();
        expect(collect(pads, seen,
                       [&] {
                           return has(seen, Event::Kind::Connected, "Nintendo Wii Remote");
                       }),
               "a Wii Remote is a pad");
        remote.press(BTN_2, 1);
        remote.press(KEY_UP, 1);
        expect(collect(pads, seen,
                       [&] {
                           return pressed(seen, Button::A) && pressed(seen, Button::Left);
                       }),
               "held sideways, its 2 is A and its d-pad turned");
        remote.press(BTN_2, 0);
        remote.press(KEY_UP, 0);

        // Its camera is part of it: where it points comes from the remote's own node.
        const FakeWiiCamera camera;
        inputs.add(camera.node());
        std::string remoteId;
        for (const Event& event : seen) {
            if (event.kind == Event::Kind::Connected && event.device == "Nintendo Wii Remote") {
                remoteId = event.source;
            }
        }
        const auto points = [&] {
            return std::ranges::any_of(seen, [&](const Event& event) {
                return event.kind == Event::Kind::Pointer && event.onScreen &&
                       event.source == remoteId && event.x > 0.7f;
            });
        };
        // The camera is opened a moment after it appears; the kernel drops a repeated value.
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        for (std::int32_t step = 0; !points() && std::chrono::steady_clock::now() < deadline;
             ++step) {
            camera.see(200 + (step % 2), 300, 384);
            std::this_thread::sleep_for(20ms);
            for (Event& event : pads.takeEvents()) {
                seen.push_back(std::move(event));
            }
        }
        expect(points(), "the remote's camera says it points right of centre");
        const auto pointerButton = [&](bool down) {
            return std::ranges::any_of(seen, [&](const Event& event) {
                return event.kind == Event::Kind::PointerButton && event.pressed == down &&
                       event.source == remoteId;
            });
        };
        remote.press(BTN_A, 1);
        expect(collect(pads, seen,
                       [&] {
                           return pointerButton(true);
                       }),
               "its A while it points is the pointer's button");
        remote.press(BTN_A, 0);
        expect(collect(pads, seen,
                       [&] {
                           return pointerButton(false);
                       }),
               "and is let go");
        expect(!pressed(seen, Button::X), "and not the pad's X");
        expect(std::ranges::none_of(pads.connected(),
                                    [](const PadInfo& info) {
                                        return info.name == "Nintendo Wii Remote IR";
                                    }),
               "and is no pad of its own");

        {
            // A Classic Controller plugged in is read as the pad, its buttons by place: B is A.
            const FakeWiiClassic classic;
            inputs.add(classic.node());
            seen.clear();
            const auto classicDeadline = std::chrono::steady_clock::now() + 2s;
            while (!pressed(seen, Button::A) &&
                   std::chrono::steady_clock::now() < classicDeadline) {
                classic.press(BTN_B, 1);
                classic.press(BTN_B, 0);
                std::this_thread::sleep_for(20ms);
                for (Event& event : pads.takeEvents()) {
                    seen.push_back(std::move(event));
                }
            }
            expect(pressed(seen, Button::A), "the Classic Controller's B is A");
            seen.clear();
            remote.press(BTN_2, 1);
            std::this_thread::sleep_for(100ms);
            for (Event& event : pads.takeEvents()) {
                seen.push_back(std::move(event));
            }
            expect(!pressed(seen, Button::A), "and the remote hanging off it is not read");
            remote.press(BTN_2, 0);
        }

        // A game that plays the real remotes itself gets them: openSU closes every part.
        seen.clear();
        static_cast<void>(pads.hold({}, WiiRemoteHold::ToGame));
        expect(collect(pads, seen,
                       [&] {
                           return has(seen, Event::Kind::Disconnected, "Nintendo Wii Remote");
                       }),
               "a Wii game on real remotes has them let go");
        expect(opened(remote.node()) == 0 && opened(camera.node()) == 0,
               "and openSU holds none of their nodes");
        {
            // A part plugged in meanwhile is told from sysfs and never opened.
            const FakeWiiClassic late;
            inputs.add(late.node());
            // Each claim waits for a pass of the reader; the second starts after the pass that
            // took the link's notice.
            pads.claim(Claim::Game);
            pads.claim(Claim::Game);
            expect(opened(late.node()) == 0, "a part plugged in during the game is not opened");
        }
        seen.clear();
        pads.release(Claim::Free);
        expect(collect(pads, seen,
                       [&] {
                           return has(seen, Event::Kind::Connected, "Nintendo Wii Remote");
                       }),
               "the game over, the remote is a pad again");
        expect(opened(remote.node()) == 1 && opened(camera.node()) == 1, "with its camera");
    }

    {
        // A DolphinBar slot's remote is a pad, sideways as a Bluetooth one is: 2 is A.
        const FakeBar bar{"slot"};
        const InputDir none{"bar"};
        EvdevPads barPads{none.path(), bar.places(), OwnVirtualPads::Read};
        FakeBarRemote remote{bar.far()};
        std::vector<Event> heard;
        const auto serveUntil = [&](const std::function<bool()>& done) {
            const auto deadline = std::chrono::steady_clock::now() + 3s;
            while (std::chrono::steady_clock::now() < deadline) {
                remote.serve();
                for (Event& event : barPads.takeEvents()) {
                    heard.push_back(std::move(event));
                }
                if (done()) {
                    return true;
                }
                std::this_thread::sleep_for(10ms);
            }
            return false;
        };
        const std::string slotId = bar.slotNode().string();
        const auto came = [&] {
            return std::ranges::any_of(heard, [&](const Event& event) {
                return event.kind == Event::Kind::Connected && event.source == slotId;
            });
        };
        expect(serveUntil(came), "the remote answering in the slot is a pad");
        expect(serveUntil([&] {
                   return remote.lights() == 0x10 && remote.mode() == 0x30;
               }),
               "its player light is lit and its buttons asked for");
        remote.report(0x00, 0x01);
        expect(serveUntil([&] {
                   return pressed(heard, Button::A);
               }),
               "its 2 is A");
        remote.report(0x00, 0x00);

        // Its Classic Controller is read as the pad: X is Y.
        remote.plug(FakeBarRemote::Extension::Classic);
        expect(serveUntil([&] {
                   return remote.mode() == 0x32;
               }),
               "a Classic Controller plugged into it is set up");
        std::array<std::uint8_t, 6> bytes = FakeBarRemote::classicIdle();
        bytes[5] = static_cast<std::uint8_t>(bytes[5] & ~0x08);
        remote.report(0x00, 0x00, bytes);
        expect(serveUntil([&] {
                   return pressed(heard, Button::Y);
               }),
               "the Classic's X is Y");
        remote.report(0x00, 0x00, FakeBarRemote::classicIdle());

        // Left to a Wii game: the slot is closed, and opened again after.
        heard.clear();
        static_cast<void>(barPads.hold({}, WiiRemoteHold::ToGame));
        expect(serveUntil([&] {
                   return has(heard, Event::Kind::Disconnected, "Nintendo Wii Remote");
               }),
               "the slot's remote is let go for a Wii game");
        expect(opened(bar.terminal()) == 1, "and the slot's node closed, the test's own end left");
        heard.clear();
        barPads.release(Claim::Free);
        expect(serveUntil(came), "the game over, the slot's remote is a pad again");
    }

    std::printf("pads: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
