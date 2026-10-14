// Pads against real devices: uinput pads stand in for controllers, and the test reads the virtual
// pad a game would read. Every Pads reads a private directory linking only the test's own nodes,
// found through sysfs, so no other controller on the machine is opened, read or grabbed.
#include "pads.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
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
using opensu::gamepad::Claim;
using opensu::gamepad::EvdevDevice;
using opensu::gamepad::Event;
using opensu::gamepad::ForwardMap;
using opensu::gamepad::OwnVirtualPads;
using opensu::gamepad::PadEvent;
using opensu::gamepad::PadInfo;
using opensu::gamepad::Pads;
using opensu::gamepad::VirtualPad;
using opensu::test::expect;
using opensu::test::fail;
namespace fs = std::filesystem;

/// The event nodes of the input devices sysfs names `name`, once each is open to this user. No
/// node is opened to find them.
std::vector<fs::path> nodesNamed(std::string_view name) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
        std::vector<fs::path> nodes;
        std::error_code error;
        for (const auto& device : fs::directory_iterator{"/sys/class/input", error}) {
            std::string named;
            std::getline(std::ifstream{device.path() / "name"}, named);
            if (named != name) {
                continue;
            }
            for (const auto& entry : fs::directory_iterator{device.path(), error}) {
                const std::string leaf = entry.path().filename().string();
                if (leaf.starts_with("event") &&
                    ::access(("/dev/input/" + leaf).c_str(), R_OK) == 0) {
                    nodes.push_back("/dev/input/" + leaf);
                }
            }
        }
        if (!nodes.empty()) {
            return nodes;
        }
        std::this_thread::sleep_for(20ms);
    }
    return {};
}

/// The one node named `name`, opened.
std::optional<EvdevDevice> openNamed(std::string_view name) {
    const std::vector<fs::path> nodes = nodesNamed(name);
    if (nodes.size() != 1) {
        return std::nullopt;
    }
    return EvdevDevice{nodes.front()};
}

/// Every node named `name`, opened.
std::vector<EvdevDevice> openAllNamed(std::string_view name) {
    std::vector<EvdevDevice> devices;
    for (const fs::path& node : nodesNamed(name)) {
        devices.emplace_back(node);
    }
    return devices;
}

/// A directory of links to the nodes a Pads may read.
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
    /// Links the node named `name` in, as udev would add it; returns the link.
    fs::path add(std::string_view name) const {
        const std::vector<fs::path> nodes = nodesNamed(name);
        expect(nodes.size() == 1, "the test's node appears once");
        const fs::path link = path_ / nodes.front().filename();
        fs::remove(link);
        fs::create_symlink(nodes.front(), link);
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
        uinput_setup setup{};
        setup.id.bustype = BUS_USB;
        setup.id.vendor = 0x054c;
        setup.id.product = 0x0ce6;
        std::snprintf(setup.name, sizeof(setup.name), "%s", name);
        expect(ioctl(fd_, UI_DEV_SETUP, &setup) == 0, "the fake motion node is set up");
        expect(ioctl(fd_, UI_DEV_CREATE) == 0, "the fake motion node is created");
    }
    FakeMotion(const FakeMotion&) = delete;
    FakeMotion& operator=(const FakeMotion&) = delete;
    ~FakeMotion() {
        ioctl(fd_, UI_DEV_DESTROY);
        ::close(fd_);
    }

    void tilt(std::int32_t value) const {
        for (const input_event out :
             {input_event{{}, EV_ABS, ABS_RX, value}, input_event{{}, EV_SYN, SYN_REPORT, 0}}) {
            expect(::write(fd_, &out, sizeof(out)) == sizeof(out), "the motion node takes input");
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
    const InputDir inputs{"shell"};
    Pads pads{inputs.path()};
    std::vector<Event> seen;

    // Connected after the reader started, as a pad switched on mid-session is.
    std::optional<FakePad> physical{std::in_place, "opensu test pad", 0x054c, 0x0ce6};
    const fs::path testLink = inputs.add("opensu test pad");
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
        inputs.add("opensu second pad");
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
        inputs.add("Steam Virtual Gamepad");
        std::this_thread::sleep_for(200ms);
        for (const Event& event : pads.takeEvents()) {
            expect(event.device != "Steam Virtual Gamepad", "Steam's virtual pad is not read");
        }
    }

    // The test pad's motion node, made before the hold as a pad brings it up with its gamepad node.
    const FakeMotion motion{"opensu test pad Motion Sensors"};
    inputs.add("opensu test pad Motion Sensors");
    std::optional<EvdevDevice> motionNode = openNamed("opensu test pad Motion Sensors");
    if (!motionNode) {
        fail("the motion node appears");
    }
    expect(!pads.hold({}).empty(), "the game is told to read only the virtual pads");
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
        for (const fs::path& node : nodesNamed(VirtualPad::name)) {
            fs::create_symlink(node, gameInputs.path() / node.filename());
        }
        Pads reader{gameInputs.path(), OwnVirtualPads::Read};
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
    expect(!pads.hold(ForwardMap{{{Button::A, {Button::B}}}, {}}).empty(),
           "the pads are held with a mapping");
    std::vector<EvdevDevice> mappedPads = openAllNamed(VirtualPad::name);
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

    std::printf("pads: all checks passed\n");
    return 0;
}
