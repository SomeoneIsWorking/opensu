#include "battery.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace opensu::device {
namespace {

namespace fs = std::filesystem;

/// The first line of a sysfs attribute, or empty when it cannot be read.
std::string attribute(const fs::path& supply, const char* name) {
    std::ifstream in{supply / name};
    std::string line;
    std::getline(in, line);
    return line;
}

std::optional<BatteryStatus> parse(const fs::path& supply) {
    if (attribute(supply, "type") != "Battery" || attribute(supply, "scope") == "Device") {
        return std::nullopt;
    }
    const std::string capacity = attribute(supply, "capacity");
    int percent = 0;
    const auto [end, error] =
        std::from_chars(capacity.data(), capacity.data() + capacity.size(), percent);
    if (capacity.empty() || error != std::errc{} || end != capacity.data() + capacity.size()) {
        return std::nullopt;
    }
    // The kernel reports Charging, Discharging, Not charging, Full or Unknown.
    return BatteryStatus{std::clamp(percent, 0, 100), attribute(supply, "status") == "Charging"};
}

} // namespace

BatteryReader::BatteryReader(std::filesystem::path root) : root_{std::move(root)} {
}

std::optional<BatteryStatus> BatteryReader::read() const {
    // A machine without a power_supply class has no battery; that is not an error.
    std::error_code error;
    std::vector<fs::path> supplies;
    for (const fs::directory_entry& entry : fs::directory_iterator{root_, error}) {
        supplies.push_back(entry.path());
    }
    std::ranges::sort(supplies);
    for (const fs::path& supply : supplies) {
        if (std::optional<BatteryStatus> status = parse(supply)) {
            return status;
        }
    }
    return std::nullopt;
}

} // namespace opensu::device
