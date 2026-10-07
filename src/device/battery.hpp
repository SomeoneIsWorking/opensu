// battery — the machine's own battery level and charging state, from sysfs.
//
// Reads /sys/class/power_supply the way the kernel documents it: a supply whose
// `type` is Battery and whose `scope` is not Device (a controller or mouse battery
// reports scope Device) is the system battery.
#pragma once

#include <filesystem>
#include <optional>

namespace iideck::device {

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

} // namespace iideck::device
