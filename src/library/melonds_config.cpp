#include "melonds_config.hpp"

#include <algorithm>
#include <array>
#include <optional>

#include "ini_file.hpp"
#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view instance{"Instance0"};
constexpr std::string_view joystick{"Instance0.Joystick"};
constexpr std::string_view unbound{"-1"};

/// melonDS's joystick code: a button's index, or `0x100 | hat << 4 | direction` for a hat, either
/// with `0x10000 | axis << 24 | direction << 20` for an axis (direction 0 positive, 1 negative).
constexpr int hat(int direction) {
    return 0x100 | direction;
}
constexpr int axis(int index, int direction) {
    return 0x10000 | (index << 24) | (direction << 20);
}

struct DefaultBinding {
    std::string_view key;
    int code;
};

// The virtual pad as SDL's joystick reads xpad: A b0, B b1, X b2, Y b3, LB b4, RB b5, Back b6,
// Start b7, hat 0, left stick axes 0 and 1. DS A east, B south, X north, Y west.
constexpr std::array defaults{
    DefaultBinding{"A", 1},
    DefaultBinding{"B", 0},
    DefaultBinding{"X", 3},
    DefaultBinding{"Y", 2},
    DefaultBinding{"L", 4},
    DefaultBinding{"R", 5},
    DefaultBinding{"Select", 6},
    DefaultBinding{"Start", 7},
    DefaultBinding{"Up", hat(0x1) | axis(1, 1)},
    DefaultBinding{"Right", hat(0x2) | axis(0, 0)},
    DefaultBinding{"Down", hat(0x4) | axis(1, 0)},
    DefaultBinding{"Left", hat(0x8) | axis(0, 1)},
};

} // namespace

std::string melondsLaunchSettings(std::string_view toml) {
    std::string out = withIniValue(toml, {instance, "JoystickID"}, "0");
    const bool bound = std::ranges::any_of(defaults, [&](const DefaultBinding& binding) {
        return iniValueIn(out, {joystick, binding.key}).value_or(std::string{unbound}) != unbound;
    });
    if (bound) {
        lucent::info("melonds", "the player's joystick buttons kept on virtual pad 0");
        return out;
    }
    for (const DefaultBinding& binding : defaults) {
        out = withIniValue(out, {joystick, binding.key}, std::to_string(binding.code));
    }
    lucent::info("melonds", "the standard layout given on virtual pad 0");
    return out;
}

std::vector<SettingsWrite> melondsWrites(const LaunchSetup& setup) {
    if (setup.padGuid.empty()) {
        return {};
    }
    std::optional<SettingsWrite> run =
        settingsWriteOf(setup.config / melondsSettingsFile, [](std::string_view toml) {
            return melondsLaunchSettings(toml);
        });
    if (!run) {
        return {};
    }
    return {std::move(*run)};
}

} // namespace opensu::library
