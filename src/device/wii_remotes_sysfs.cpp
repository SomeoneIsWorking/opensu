#include "wii_remotes.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include <unistd.h>

namespace opensu::device {
namespace {

namespace fs = std::filesystem;

constexpr std::uint16_t usbBus = 0x0003;
constexpr std::uint16_t bluetoothBus = 0x0005;
constexpr std::uint16_t nintendo = 0x057e;
// RVL-CNT-01 and RVL-CNT-01-TR (Motion Plus inside).
constexpr std::uint16_t wiiRemote = 0x0306;
constexpr std::uint16_t wiiRemotePlus = 0x0330;
// DragonRise's id for the bar's gamepad and IR mouse mode (the kernel's hid-mf).
constexpr std::uint16_t dragonRise = 0x0079;
constexpr std::uint16_t dolphinBarGamepad = 0x1803;

struct HidDevice {
    std::uint16_t bus{0};
    std::uint16_t vendor{0};
    std::uint16_t product{0};
    std::string phys;
};

std::optional<std::uint16_t> hexOf(std::string_view text) {
    std::uint32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size() ||
        value > 0xffff) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(value);
}

/// `HID_ID=BBBB:VVVVVVVV:PPPPPPPP` and `HID_PHYS` of a device's uevent; nothing when unread.
std::optional<HidDevice> deviceOf(const fs::path& folder) {
    std::ifstream in{folder / "uevent"};
    HidDevice device;
    bool identified = false;
    std::string line;
    while (std::getline(in, line)) {
        const std::string_view text{line};
        if (text.starts_with("HID_ID=")) {
            const std::string_view id = text.substr(7);
            const std::size_t first = id.find(':');
            const std::size_t second = id.find(':', first + 1);
            if (first == std::string_view::npos || second == std::string_view::npos) {
                return std::nullopt;
            }
            const auto bus = hexOf(id.substr(0, first));
            const auto vendor = hexOf(id.substr(first + 1, second - first - 1));
            const auto product = hexOf(id.substr(second + 1));
            if (!bus || !vendor || !product) {
                return std::nullopt;
            }
            device.bus = *bus;
            device.vendor = *vendor;
            device.product = *product;
            identified = true;
        } else if (text.starts_with("HID_PHYS=")) {
            device.phys = line.substr(9);
        }
    }
    if (!identified) {
        return std::nullopt;
    }
    return device;
}

/// The USB device a HID interface belongs to: its phys without the `/inputN` interface.
std::string usbDeviceOf(const std::string& phys) {
    return phys.substr(0, phys.rfind("/input"));
}

/// The slot a bar's HID interface is: the `N` of its phys's `/inputN`.
int slotOf(const std::string& phys) {
    const std::size_t at = phys.rfind("/input");
    int slot = 0;
    if (at != std::string::npos) {
        const std::string_view number = std::string_view{phys}.substr(at + 6);
        std::from_chars(number.data(), number.data() + number.size(), slot);
    }
    return slot;
}

/// Whether this user can read and write each hidraw node the HID device at `folder` has.
bool reachable(const fs::path& folder, const fs::path& dev) {
    std::error_code failed;
    bool any = false;
    for (const fs::directory_entry& node : fs::directory_iterator{folder / "hidraw", failed}) {
        any = true;
        if (::access((dev / node.path().filename()).c_str(), R_OK | W_OK) != 0) {
            return false;
        }
    }
    return any;
}

} // namespace

WiiRemotes findWiiRemotes(const HidPlaces& places) {
    WiiRemotes found;
    std::set<std::string> bars;
    std::set<std::string> otherBars;
    std::vector<std::pair<std::string, BarSlot>> slots;
    std::error_code failed;
    for (const fs::directory_entry& entry : fs::directory_iterator{places.devices, failed}) {
        const std::optional<HidDevice> device = deviceOf(entry.path());
        if (!device) {
            continue;
        }
        const bool remote = device->vendor == nintendo &&
                            (device->product == wiiRemote || device->product == wiiRemotePlus);
        if (remote && device->bus == bluetoothBus) {
            ++found.bluetoothRemotes;
        } else if (remote && device->bus == usbBus) {
            bars.insert(usbDeviceOf(device->phys));
            found.barsReachable = found.barsReachable && reachable(entry.path(), places.dev);
            for (const fs::directory_entry& node :
                 fs::directory_iterator{entry.path() / "hidraw", failed}) {
                slots.push_back(
                    {usbDeviceOf(device->phys), BarSlot{.node = places.dev / node.path().filename(),
                                                        .index = slotOf(device->phys)}});
            }
        } else if (device->bus == usbBus && device->vendor == dragonRise &&
                   device->product == dolphinBarGamepad) {
            otherBars.insert(usbDeviceOf(device->phys));
        }
    }
    std::ranges::sort(slots, [](const auto& one, const auto& other) {
        return std::tie(one.first, one.second.index) < std::tie(other.first, other.second.index);
    });
    for (auto& [bar, slot] : slots) {
        found.barSlots.push_back(std::move(slot));
    }
    found.dolphinBars = static_cast<int>(bars.size());
    found.otherModeBars = static_cast<int>(otherBars.size());
    return found;
}

} // namespace opensu::device
