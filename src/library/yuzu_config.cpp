#include "yuzu_config.hpp"

#include <algorithm>
#include <optional>

#include "ini_file.hpp"
#include "lucent/log.h"
#include "qt_config_ini.hpp"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view controls{"Controls"};
constexpr std::string_view sdlEngine{"engine:sdl"};

/// A Pro Controller's binding, Eden's key after `player_<N>_`, as Eden's own mapping of a pad of
/// xpad's layout writes it; `{}` stands for `guid:<GUID>,port:<N>`.
struct DefaultBinding {
    std::string_view key;
    std::string_view value;
};

// Eden's positional layout: Switch A east (xpad B, b1), B south, X north (xpad Y, b3), Y west.
constexpr std::array defaults{
    DefaultBinding{"button_a", "button:1"},
    DefaultBinding{"button_b", "button:0"},
    DefaultBinding{"button_x", "button:3"},
    DefaultBinding{"button_y", "button:2"},
    DefaultBinding{"button_lstick", "button:9"},
    DefaultBinding{"button_rstick", "button:10"},
    DefaultBinding{"button_l", "button:4"},
    DefaultBinding{"button_r", "button:5"},
    DefaultBinding{"button_zl", "threshold:0.500000,axis:2,invert:+"},
    DefaultBinding{"button_zr", "threshold:0.500000,axis:5,invert:+"},
    DefaultBinding{"button_plus", "button:7"},
    DefaultBinding{"button_minus", "button:6"},
    DefaultBinding{"button_dleft", "hat:0,direction:left"},
    DefaultBinding{"button_dup", "hat:0,direction:up"},
    DefaultBinding{"button_dright", "hat:0,direction:right"},
    DefaultBinding{"button_ddown", "hat:0,direction:down"},
    DefaultBinding{"button_slleft", "button:4"},
    DefaultBinding{"button_srleft", "button:5"},
    DefaultBinding{"button_home", "button:8"},
    DefaultBinding{"button_screenshot", ""},
    DefaultBinding{"button_slright", "button:4"},
    DefaultBinding{"button_srright", "button:5"},
    DefaultBinding{"lstick", "axis_x:0,axis_y:1,deadzone:0.150000,offset_x:0.000000,"
                             "offset_y:0.000000,invert_x:+,invert_y:+"},
    DefaultBinding{"rstick", "axis_x:3,axis_y:4,deadzone:0.150000,offset_x:0.000000,"
                             "offset_y:0.000000,invert_x:+,invert_y:+"},
    DefaultBinding{"motionleft", ""},
    DefaultBinding{"motionright", ""},
};

constexpr std::string_view emptyBinding{"[empty]"};
/// Eden's `ControllerType::ProController`.
constexpr std::string_view proController{"0"};

/// `key`'s value under `[Controls]`; empty when it is not there.
std::string control(std::string_view ini, const std::string& key) {
    return iniValueIn(ini, {controls, key}).value_or("");
}

/// Whether Eden reads `key` as true: its `\default` says to take `fallback`.
bool isTrue(std::string_view ini, const std::string& key, bool fallback) {
    if (iniValueIn(ini, {controls, key + "\\default"}).value_or("true") == "true") {
        return fallback;
    }
    return control(ini, key) == "true";
}

std::string defaultBinding(std::string_view value, std::string_view guid, int port) {
    if (value.empty()) {
        return std::string{emptyBinding};
    }
    return qtQuoted(std::string{sdlEngine} + ",guid:" + std::string{guid} +
                    ",port:" + std::to_string(port) + "," + std::string{value});
}

std::string withPlayer(std::string ini, int player, std::string_view guid) {
    const std::string prefix = "player_" + std::to_string(player) + "_";
    const bool hasSdl = std::ranges::any_of(defaults, [&](const DefaultBinding& binding) {
        return isSdlBinding(qtUnquoted(control(ini, prefix + std::string{binding.key})));
    });
    if (hasSdl) {
        for (const DefaultBinding& binding : defaults) {
            const std::string key = prefix + std::string{binding.key};
            const std::string value = control(ini, key);
            if (isSdlBinding(qtUnquoted(value))) {
                ini = withQtSetting(ini, {controls, key},
                                    qtQuoted(withSdlPad(qtUnquoted(value), guid, player)));
            }
        }
        lucent::info("eden", "player {} bound to virtual pad port {}", player + 1, player);
    } else if (player == 0 || isTrue(ini, prefix + "connected", false)) {
        for (const DefaultBinding& binding : defaults) {
            const std::string key = prefix + std::string{binding.key};
            ini = withQtSetting(ini, {controls, key}, defaultBinding(binding.value, guid, player));
        }
        const std::string type = prefix + "type";
        ini = withQtSetting(ini, {controls, type}, proController);
        lucent::info("eden", "player {} given a Pro Controller on virtual pad port {}", player + 1,
                     player);
    } else {
        return ini;
    }
    const std::string connected = prefix + "connected";
    return withQtSetting(ini, {controls, connected}, "true");
}

} // namespace

std::string yuzuGuid(std::string_view sdlGuid) {
    std::string guid{sdlGuid};
    if (guid.size() == 32) {
        guid.replace(4, 4, "0000");
    }
    return guid;
}

std::string yuzuLaunchSettings(std::string_view ini, std::string_view padGuid) {
    std::string out = withQtSetting(ini, {"UI", "check_for_updates"}, "false");
    if (padGuid.empty()) {
        return out;
    }
    const std::string guid = yuzuGuid(padGuid);
    for (int player = 0; player < yuzuPlayers; ++player) {
        out = withPlayer(std::move(out), player, guid);
    }
    return out;
}

std::vector<fs::path> yuzuFiles(const LaunchSetup& setup) {
    return {setup.config / yuzuSettingsFile};
}

std::vector<RunFile> yuzuRunFiles(const LaunchSetup& setup) {
    std::optional<RunFile> run =
        runFileOf(setup.config / yuzuSettingsFile, [&](std::string_view player) {
            return yuzuLaunchSettings(player, setup.padGuid);
        });
    if (!run) {
        return {};
    }
    return {std::move(*run)};
}

} // namespace opensu::library
