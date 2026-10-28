// battery — the machine's own battery level and charging state, from SDL's power info on every OS
// (on Linux SDL asks UPower, then /sys/class/power_supply, leaving device batteries out), and a
// wireless controller's from sysfs on Linux (`controller_batteries_sysfs.cpp`); none elsewhere
// (`controller_batteries_none.cpp`).
#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include <SDL3/SDL_power.h>

namespace opensu::device {

struct BatteryStatus {
    /// 0 to 100.
    int percent{};
    bool charging{false};
};

class BatteryReader {
  public:
    /// The system battery, or nothing when the machine has none or its level is unknown.
    [[nodiscard]] std::optional<BatteryStatus> read() const;

    /// What SDL's `state` and `percent` (-1 when unknown) say of the system battery.
    [[nodiscard]] static std::optional<BatteryStatus> statusOf(SDL_PowerState state, int percent);
};

/// The battery of a wireless controller, found by the controller's unique id (its Bluetooth
/// address): the kernel names such a supply after it ("ps-controller-battery-e8:48:b8:c8:20:00"),
/// with `type` Battery and `scope` Device.
class ControllerBatteries {
  public:
    /// The system's: /sys/class/power_supply on Linux.
    ControllerBatteries();
    /// The supplies under `root`.
    explicit ControllerBatteries(std::filesystem::path root);

    /// The battery of the controller with `uniq`, or nothing for an empty id or no match. Case and
    /// the separators between address bytes do not matter.
    [[nodiscard]] std::optional<BatteryStatus> read(std::string_view uniq) const;

  private:
    std::filesystem::path root_;
};

} // namespace opensu::device
