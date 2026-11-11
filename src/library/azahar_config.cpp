#include "azahar_config.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include "ini_file.hpp"
#include "lucent/log.h"
#include "qt_config_ini.hpp"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view controls{"Controls"};

/// A 3DS input's key in a profile and its binding on a pad of xpad's layout, SDL game controller
/// buttons and axes by number; `{}` stands for the pad's `engine`, `guid` and `port`.
struct DefaultBinding {
    std::string_view key;
    std::string_view value;
};

// Positional, as Azahar's automap of an Xbox pad: 3DS A east (SDL B), B south, X north, Y west.
constexpr std::array defaults{
    DefaultBinding{"button_a", "button:1"},
    DefaultBinding{"button_b", "button:0"},
    DefaultBinding{"button_x", "button:3"},
    DefaultBinding{"button_y", "button:2"},
    DefaultBinding{"button_up", "button:11"},
    DefaultBinding{"button_down", "button:12"},
    DefaultBinding{"button_left", "button:13"},
    DefaultBinding{"button_right", "button:14"},
    DefaultBinding{"button_l", "button:9"},
    DefaultBinding{"button_r", "button:10"},
    DefaultBinding{"button_start", "button:6"},
    DefaultBinding{"button_select", "button:4"},
    DefaultBinding{"button_zl", "axis:4,direction:+,threshold:0.500000"},
    DefaultBinding{"button_zr", "axis:5,direction:+,threshold:0.500000"},
    DefaultBinding{"button_home", "button:5"},
    DefaultBinding{"circle_pad", "axis_x:0,axis_y:1"},
    DefaultBinding{"c_stick", "axis_x:2,axis_y:3"},
};

/// The first virtual pad: one 3DS, one player.
constexpr int padPort{0};

std::string control(std::string_view ini, const std::string& key) {
    return iniValueIn(ini, {controls, key}).value_or("");
}

int number(std::string_view ini, const std::string& key) {
    if (iniValueIn(ini, {controls, key + "\\default"}).value_or("false") == "true") {
        return 0;
    }
    const std::string value = control(ini, key);
    int out = 0;
    for (const char digit : value) {
        if (digit < '0' || digit > '9') {
            return 0;
        }
        out = out * 10 + (digit - '0');
    }
    return out;
}

std::string defaultBinding(std::string_view value, std::string_view guid) {
    return qtQuoted("api:controller,engine:sdl,guid:" + std::string{guid} +
                    ",port:" + std::to_string(padPort) + "," + std::string{value});
}

/// `ini` with its active profile on the first virtual pad; the profile is made when there is none.
std::string withProfile(std::string ini, std::string_view guid) {
    const int profiles = number(ini, "profiles\\size");
    // Azahar clamps the current profile to the ones there are.
    const int active = std::clamp(number(ini, "profile"), 0, std::max(profiles, 1) - 1);
    const std::string prefix = "profiles\\" + std::to_string(active + 1) + "\\";
    const bool hasSdl = std::ranges::any_of(defaults, [&](const DefaultBinding& binding) {
        return isSdlBinding(qtUnquoted(control(ini, prefix + std::string{binding.key})));
    });
    for (const DefaultBinding& binding : defaults) {
        const std::string key = prefix + std::string{binding.key};
        const std::string value = qtUnquoted(control(ini, key));
        if (!hasSdl) {
            ini = withQtSetting(ini, {controls, key}, defaultBinding(binding.value, guid));
        } else if (isSdlBinding(value)) {
            ini = withQtSetting(ini, {controls, key}, qtQuoted(withSdlPad(value, guid, padPort)));
        }
    }
    if (profiles == 0) {
        ini = withIniValue(ini, {controls, "profiles\\size"}, "1");
    }
    lucent::info("azahar", "profile {} {} virtual pad port {}", active + 1,
                 hasSdl ? "bound to" : "given the standard layout on", padPort);
    return ini;
}

} // namespace

std::string azaharLaunchSettings(std::string_view ini, std::string_view padGuid) {
    std::string out = withQtSetting(ini, {"Miscellaneous", "check_for_update_on_start"}, "false");
    if (padGuid.empty()) {
        return out;
    }
    return withProfile(std::move(out), padGuid);
}

std::vector<SettingsWrite> azaharWrites(const LaunchSetup& setup) {
    std::optional<SettingsWrite> run =
        settingsWriteOf(setup.config / azaharSettingsFile, [&setup](std::string_view player) {
            return azaharLaunchSettings(player, setup.padGuid);
        });
    if (!run) {
        return {};
    }
    return {std::move(*run)};
}

} // namespace opensu::library
