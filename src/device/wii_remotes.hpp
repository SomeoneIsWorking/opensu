// wii_remotes — the real Wii Remotes this machine has, from the kernel's HID devices. A Mayflash
// DolphinBar in its Wii Remote mode (mode 4) is one USB device 057e:0306 with four HID slots, each
// bound by hid-generic and there whether or not a remote is synced to it; in its gamepad and IR
// mouse mode (mode 3) it is 0079:1803. A remote connected over Bluetooth is bus 0005 with 057e:0306
// or 057e:0330, bound by hid-wiimote. Dolphin reaches a remote through the slot's hidraw node.
#pragma once

#include <filesystem>

namespace opensu::device {

/// Where the HID devices are read from.
struct HidPlaces {
    /// The HID devices' sysfs folders, each with its `uevent`.
    std::filesystem::path devices{"/sys/bus/hid/devices"};
    /// Where their hidraw nodes are.
    std::filesystem::path dev{"/dev"};

    /// The system's, or in a hidden run only `<dataDir>/hidden-hid`'s `devices` and `dev`, which
    /// nothing is unless a maintainer tool puts a fake there.
    [[nodiscard]] static HidPlaces of(const std::filesystem::path& dataDir, bool hidden) {
        if (!hidden) {
            return {};
        }
        const std::filesystem::path root = dataDir / "hidden-hid";
        return {.devices = root / "devices", .dev = root / "dev"};
    }
};

/// The real Wii Remotes there are now.
struct WiiRemotes {
    /// DolphinBars in Wii Remote mode.
    int dolphinBars{0};
    /// Whether this user can read and write every slot of them, which Dolphin needs.
    bool barsReachable{true};
    /// DolphinBars in another mode, which Wii games cannot use.
    int otherModeBars{0};
    /// Remotes connected over Bluetooth.
    int bluetoothRemotes{0};

    /// Whether a Wii game plays real remotes: a bar in Wii Remote mode, or a Bluetooth remote.
    [[nodiscard]] bool present() const noexcept {
        return dolphinBars > 0 || bluetoothRemotes > 0;
    }

    bool operator==(const WiiRemotes&) const = default;
};

/// The real Wii Remotes `places` show; none off Linux (`wii_remotes_none.cpp`).
[[nodiscard]] WiiRemotes findWiiRemotes(const HidPlaces& places);

} // namespace opensu::device
