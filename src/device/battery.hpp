// battery — the machine's own battery level and charging state, from sysfs.
//
// Reads /sys/class/power_supply the way the kernel documents it: a supply whose
// `type` is Battery and whose `scope` is not Device (a controller or mouse battery
// reports scope Device) is the system battery.
#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace opensu::device {

struct BatteryStatus {
    /// 0 to 100.
    int percent{};
    bool charging{false};
};

class BatteryReader {
  public:
    explicit BatteryReader(std::filesystem::path root = "/sys/class/power_supply");

    /// The first system battery by supply name, or nothing when the machine has none.
    [[nodiscard]] std::optional<BatteryStatus> read() const;

  private:
    std::filesystem::path root_;
};

/// The battery of a wireless controller, found by the controller's unique id (its Bluetooth
/// address): the kernel names such a supply after it ("ps-controller-battery-e8:48:b8:c8:20:00").
class ControllerBatteries {
  public:
    explicit ControllerBatteries(std::filesystem::path root = "/sys/class/power_supply");

    /// The battery of the controller with `uniq`, or nothing for an empty id or no match. Case and
    /// the separators between address bytes do not matter.
    [[nodiscard]] std::optional<BatteryStatus> read(std::string_view uniq) const;

  private:
    std::filesystem::path root_;
};

} // namespace opensu::device
