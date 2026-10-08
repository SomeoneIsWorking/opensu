// BatteryReader against a fake /sys/class/power_supply.
#include "device/battery.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {

namespace fs = std::filesystem;
using iideck::device::BatteryReader;
using iideck::device::BatteryStatus;

[[noreturn]] void fail(const char* what) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::exit(1);
}

template <class T> const T& need(const std::optional<T>& value, const char* what) {
    if (!value) {
        fail(what);
    }
    return *value;
}

void expect(bool condition, const char* what) {
    if (!condition) {
        fail(what);
    }
}

/// A fresh fake power_supply class directory.
fs::path freshRoot(const char* name) {
    const fs::path root = fs::path{IIDECK_TEST_SCRATCH} / "battery" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void supply(const fs::path& root, const char* name, const char* type, const char* scope,
            const char* capacity, const char* status) {
    const fs::path dir = root / name;
    fs::create_directories(dir);
    const auto write = [&dir](const char* attribute, const char* value) {
        if (value != nullptr) {
            std::ofstream{dir / attribute} << value << '\n';
        }
    };
    write("type", type);
    write("scope", scope);
    write("capacity", capacity);
    write("status", status);
}

void systemBattery() {
    const fs::path root = freshRoot("system");
    supply(root, "AC", "Mains", nullptr, nullptr, nullptr);
    supply(root, "BAT0", "Battery", nullptr, "73", "Discharging");
    const std::optional<BatteryStatus> status = BatteryReader{root}.read();
    expect(status && status->percent == 73 && !status->charging, "BAT0 at 73%, discharging");
}

void charging() {
    const fs::path root = freshRoot("charging");
    supply(root, "BAT1", "Battery", "System", "100", "Charging");
    const std::optional<BatteryStatus> status = BatteryReader{root}.read();
    expect(status && status->percent == 100 && status->charging, "Charging is charging");

    const fs::path full = freshRoot("full");
    supply(full, "BAT0", "Battery", nullptr, "100", "Full");
    expect(!need(BatteryReader{full}.read(), "Full reads").charging, "Full is not charging");
}

void deviceBatteriesIgnored() {
    const fs::path root = freshRoot("device");
    supply(root, "hidpp_battery_0", "Battery", "Device", "40", "Discharging");
    expect(!BatteryReader{root}.read(), "a mouse battery is not the system battery");
}

void unreadable() {
    const fs::path root = freshRoot("unreadable");
    supply(root, "BAT0", "Battery", nullptr, "abc", "Discharging");
    expect(!BatteryReader{root}.read(), "a capacity that is not a number is no battery");
    supply(root, "BAT1", "Battery", nullptr, "140", "Discharging");
    expect(need(BatteryReader{root}.read(), "a clamped capacity reads").percent == 100,
           "capacity is clamped to 100");
    expect(!BatteryReader{root / "missing"}.read(), "no power_supply class, no battery");
}

void firstByName() {
    const fs::path root = freshRoot("order");
    supply(root, "BAT1", "Battery", nullptr, "20", "Discharging");
    supply(root, "BAT0", "Battery", nullptr, "60", "Discharging");
    expect(need(BatteryReader{root}.read(), "a battery reads").percent == 60,
           "the first battery by name");
}

} // namespace

int main() {
    systemBattery();
    charging();
    deviceBatteriesIgnored();
    unreadable();
    firstByName();
    std::printf("battery: all checks passed\n");
    return 0;
}
