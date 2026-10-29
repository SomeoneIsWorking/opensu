// The system battery from SDL's power states, and ControllerBatteries against a fake
// /sys/class/power_supply.
#include "device/battery.hpp"
#include "support/test_main.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {

namespace fs = std::filesystem;
using opensu::device::BatteryReader;
using opensu::device::BatteryStatus;
using opensu::device::ControllerBatteries;

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
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "battery" / name;
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
    const auto status = BatteryReader::statusOf;
    expect(!status(SDL_POWERSTATE_NO_BATTERY, -1), "a desktop has no battery");
    expect(!status(SDL_POWERSTATE_UNKNOWN, 50), "an unknown state is no battery");
    expect(!status(SDL_POWERSTATE_ERROR, -1), "an error is no battery");
    expect(!status(SDL_POWERSTATE_ON_BATTERY, -1), "an unknown level is no battery");
    const BatteryStatus draining = need(status(SDL_POWERSTATE_ON_BATTERY, 42), "draining reads");
    expect(draining.percent == 42 && !draining.charging, "on battery is not charging");
    expect(need(status(SDL_POWERSTATE_CHARGING, 80), "charging reads").charging, "charging");
    expect(!need(status(SDL_POWERSTATE_CHARGED, 100), "charged reads").charging,
           "a full battery is not charging");
    expect(need(status(SDL_POWERSTATE_ON_BATTERY, 120), "a clamped level reads").percent == 100,
           "the level is clamped to 100");
}

void controllerBattery() {
    const fs::path root = freshRoot("controller");
    supply(root, "BAT0", "Battery", "System", "90", "Discharging");
    supply(root, "hidpp_battery_0", "Battery", "Device", "40", "Discharging");
    supply(root, "ps-controller-battery-e8:48:b8:c8:20:00", "Battery", "Device", "55", "Charging");
    supply(root, "nintendo_switch_controller_battery_86:10:67:67:72:5f", "Battery", "Device", "30",
           "Discharging");
    const ControllerBatteries batteries{root};
    const std::optional<BatteryStatus> ps = batteries.read("E8:48:B8:C8:20:00");
    expect(ps && ps->percent == 55 && ps->charging, "a DualSense battery by address, any case");
    const std::optional<BatteryStatus> joy = batteries.read("86:10:67:67:72:5F");
    expect(joy && joy->percent == 30, "a Switch pad's battery, underscores and colons alike");
    expect(!batteries.read("aa:bb:cc:dd:ee:ff"), "an unrelated pad has no battery");
    expect(!batteries.read(""), "a pad without an id has no battery");
    expect(!ControllerBatteries{root / "missing"}.read("e8:48:b8:c8:20:00"),
           "no power_supply class, no battery");
    expect(!batteries.read("BAT0"), "the system battery is no controller's");
}

} // namespace

int main() try {
    controllerBattery();
    systemBattery();
    std::printf("battery: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
