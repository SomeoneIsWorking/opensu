#include "battery.hpp"

#include <algorithm>
#include <cctype>
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

std::optional<BatteryStatus> parse(const fs::path& supply, bool controller) {
    if (attribute(supply, "type") != "Battery" ||
        (attribute(supply, "scope") == "Device") != controller) {
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

/// `text` lowercased with everything but letters and digits dropped.
std::string squeezed(std::string_view text) {
    std::string out;
    for (const char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return out;
}

/// The supply directories under `root`, by name.
std::vector<fs::path> suppliesUnder(const fs::path& root) {
    std::error_code error;
    std::vector<fs::path> supplies;
    for (const fs::directory_entry& entry : fs::directory_iterator{root, error}) {
        supplies.push_back(entry.path());
    }
    std::ranges::sort(supplies);
    return supplies;
}

} // namespace

BatteryReader::BatteryReader(std::filesystem::path root) : root_{std::move(root)} {
}

std::optional<BatteryStatus> BatteryReader::read() const {
    // A machine without a power_supply class has no battery; that is not an error.
    for (const fs::path& supply : suppliesUnder(root_)) {
        if (std::optional<BatteryStatus> status = parse(supply, false)) {
            return status;
        }
    }
    return std::nullopt;
}

ControllerBatteries::ControllerBatteries(std::filesystem::path root) : root_{std::move(root)} {
}

std::optional<BatteryStatus> ControllerBatteries::read(std::string_view uniq) const {
    const std::string wanted = squeezed(uniq);
    if (wanted.empty()) {
        return std::nullopt;
    }
    for (const fs::path& supply : suppliesUnder(root_)) {
        if (squeezed(supply.filename().string()).find(wanted) == std::string::npos) {
            continue;
        }
        if (std::optional<BatteryStatus> status = parse(supply, true)) {
            return status;
        }
    }
    return std::nullopt;
}

} // namespace opensu::device
