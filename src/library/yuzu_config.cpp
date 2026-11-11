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
/// Eden's `ControllerType::ProController` and `ControllerType::Handheld`.
constexpr std::string_view proController{"0"};
constexpr std::string_view handheldType{"4"};

std::string playerPrefix(int index) {
    return "player_" + std::to_string(index) + "_";
}

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

/// One of Eden's controllers: its `player_<index>_` settings, the virtual pad it plays, the type it
/// is given without SDL bindings, and whether it is to be connected for the run.
struct Slot {
    std::string shown;
    int index;
    int port;
    std::string_view type;
    /// Without SDL bindings, given the standard layout.
    bool given;
    /// With SDL bindings, connected.
    bool connects;
};

std::string withSlot(std::string ini, const Slot& slot, std::string_view guid) {
    const std::string prefix = playerPrefix(slot.index);
    const std::string connected = prefix + "connected";
    const bool hasSdl = std::ranges::any_of(defaults, [&](const DefaultBinding& binding) {
        return isSdlBinding(qtUnquoted(control(ini, prefix + std::string{binding.key})));
    });
    if (hasSdl) {
        for (const DefaultBinding& binding : defaults) {
            const std::string key = prefix + std::string{binding.key};
            const std::string value = control(ini, key);
            if (isSdlBinding(qtUnquoted(value))) {
                ini = withQtSetting(ini, {controls, key},
                                    qtQuoted(withSdlPad(qtUnquoted(value), guid, slot.port)));
            }
        }
        lucent::info("eden", "{} bound to virtual pad port {}", slot.shown, slot.port);
        return slot.connects ? withQtSetting(ini, {controls, connected}, "true") : ini;
    }
    if (!slot.given) {
        return ini;
    }
    for (const DefaultBinding& binding : defaults) {
        const std::string key = prefix + std::string{binding.key};
        ini = withQtSetting(ini, {controls, key}, defaultBinding(binding.value, guid, slot.port));
    }
    const std::string type = prefix + "type";
    ini = withQtSetting(ini, {controls, type}, slot.type);
    lucent::info("eden", "{} given the standard layout on virtual pad port {}", slot.shown,
                 slot.port);
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
    // Player 1 plays the handheld while it is connected, as Eden sets it for a Handheld player 1.
    const bool handheld = isTrue(out, playerPrefix(yuzuHandheld) + "connected", false);
    out = withSlot(std::move(out),
                   Slot{.shown = "the handheld",
                        .index = yuzuHandheld,
                        .port = 0,
                        .type = handheldType,
                        .given = handheld,
                        .connects = false},
                   guid);
    for (int player = 0; player < yuzuPlayers; ++player) {
        const bool first = player == 0;
        const bool connected = isTrue(out, playerPrefix(player) + "connected", first);
        out = withSlot(std::move(out),
                       Slot{.shown = "player " + std::to_string(player + 1),
                            .index = player,
                            .port = player,
                            .type = proController,
                            .given = first ? !handheld : connected,
                            .connects = !first || !handheld},
                       guid);
    }
    return out;
}

std::vector<SettingsWrite> yuzuWrites(const LaunchSetup& setup) {
    std::optional<SettingsWrite> run =
        settingsWriteOf(setup.config / yuzuSettingsFile, [&](std::string_view player) {
            return yuzuLaunchSettings(player, setup.padGuid);
        });
    if (!run) {
        return {};
    }
    return {std::move(*run)};
}

} // namespace opensu::library
