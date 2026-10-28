#include "battery.hpp"

namespace opensu::device {

ControllerBatteries::ControllerBatteries() = default;

ControllerBatteries::ControllerBatteries(std::filesystem::path root) : root_{std::move(root)} {
}

std::optional<BatteryStatus> ControllerBatteries::read(std::string_view /*uniq*/) const {
    return std::nullopt;
}

} // namespace opensu::device
