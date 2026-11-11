#include "ryujinx_config.hpp"

#include <array>
#include <optional>
#include <utility>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

constexpr std::string_view gamepadBackend{"GamepadSDL2"};
constexpr std::string_view keyboardBackend{"WindowKeyboard"};

/// An SDL3 build's enum name and the SDL2 one every build reads.
struct Rename {
    std::string_view sdl3;
    std::string_view sdl2;
};

constexpr Rename audioRename{.sdl3 = "SDL3", .sdl2 = "SDL2"};
constexpr Rename backendRename{.sdl3 = "GamepadSDL3", .sdl2 = gamepadBackend};

/// What Ryubing 1.3.3 writes on its first start (`LoadDefault`) on a Vulkan host, its keyboard
/// player 1 left out: Ryujinx reads every field of the file, so a partial one would not load.
constexpr std::string_view firstStartSettings = R"json({
  "version": 70,
  "enable_file_log": true,
  "backend_threading": "Auto",
  "res_scale": 1,
  "res_scale_custom": 1,
  "max_anisotropy": -1,
  "aspect_ratio": "Fixed16x9",
  "anti_aliasing": "None",
  "scaling_filter": "Bilinear",
  "scaling_filter_level": 80,
  "graphics_shaders_dump_path": "",
  "logging_enable_debug": false,
  "logging_enable_stub": true,
  "logging_enable_info": true,
  "logging_enable_warn": true,
  "logging_enable_error": true,
  "logging_enable_trace": false,
  "logging_enable_guest": true,
  "logging_enable_fs_access_log": false,
  "logging_enable_avalonia": false,
  "logging_filtered_classes": [],
  "logging_graphics_debug_level": "None",
  "system_language": "AmericanEnglish",
  "system_region": "USA",
  "system_time_zone": "UTC",
  "system_time_offset": 0,
  "match_system_time": false,
  "use_input_global_config": false,
  "docked_mode": true,
  "enable_discord_integration": true,
  "check_updates_on_start": false,
  "update_checker_type": "PromptAtStartup",
  "focus_lost_action_type": "DoNothing",
  "show_confirm_exit": true,
  "ignore_applet": false,
  "skip_user_profiles": false,
  "remember_window_state": true,
  "show_title_bar": true,
  "enable_hardware_acceleration": true,
  "hide_cursor": 1,
  "enable_vsync": false,
  "vsync_mode": 0,
  "enable_custom_vsync_interval": false,
  "custom_vsync_interval": 120,
  "enable_shader_cache": true,
  "enable_texture_recompression": false,
  "enable_macro_hle": true,
  "enable_color_space_passthrough": false,
  "enable_ptc": true,
  "enable_low_power_ptc": false,
  "tick_scalar": 0,
  "enable_internet_access": false,
  "enable_fs_integrity_checks": true,
  "fs_global_access_log_mode": 0,
  "audio_backend": "SDL2",
  "audio_volume": 1,
  "memory_manager_mode": "HostMappedUnsafe",
  "dram_size": 0,
  "ignore_missing_services": false,
  "gui_columns": {
    "fav_column": true,
    "icon_column": true,
    "app_column": true,
    "dev_column": true,
    "version_column": true,
    "ldn_info_column": false,
    "time_played_column": true,
    "last_played_column": true,
    "file_ext_column": true,
    "file_size_column": true,
    "path_column": true
  },
  "column_sort": {
    "sort_column_id": 0,
    "sort_ascending": false
  },
  "game_dirs": [],
  "autoload_dirs": [],
  "shown_file_types": {
    "nsp": true,
    "pfs0": true,
    "xci": true,
    "nca": true,
    "nro": true,
    "nso": true
  },
  "window_startup": {
    "window_size_width": 1280,
    "window_size_height": 760,
    "window_position_x": 0,
    "window_position_y": 0,
    "window_maximized": false
  },
  "language_code": "en_US",
  "base_style": "Dark",
  "game_list_view_mode": 0,
  "show_names": true,
  "grid_size": 2,
  "application_sort": 0,
  "is_ascending_order": true,
  "start_fullscreen": false,
  "start_no_ui": false,
  "show_console": true,
  "enable_keyboard": false,
  "enable_mouse": false,
  "disable_input_when_out_of_focus": false,
  "hotkeys": {
    "toggle_vsync_mode": "F1",
    "screenshot": "F8",
    "show_ui": "F4",
    "pause": "F5",
    "toggle_mute": "F2",
    "res_scale_up": "Unbound",
    "res_scale_down": "Unbound",
    "volume_up": "Unbound",
    "volume_down": "Unbound",
    "custom_vsync_interval_increment": "Unbound",
    "custom_vsync_interval_decrement": "Unbound",
    "turbo_mode": "Unbound",
    "turbo_mode_while_held": false
  },
  "input_config": [],
  "rainbow_speed": 1,
  "graphics_backend": "Vulkan",
  "preferred_gpu": "",
  "multiplayer_mode": 0,
  "multiplayer_lan_interface_id": "0",
  "multiplayer_disable_p2p": false,
  "multiplayer_ldn_passphrase": "",
  "ldn_server": "",
  "use_hypervisor": true,
  "enable_gdb_stub": false,
  "gdb_stub_port": 55555,
  "debugger_suspend_on_start": false,
  "show_dirty_hacks": false,
  "dirty_hacks": []
})json";

bool isText(const Json& value, std::string_view text) {
    return value.is_string() && value.get_ref<const std::string&>() == text;
}

void rename(Json& value, const Rename& names) {
    if (isText(value, names.sdl3)) {
        value = names.sdl2;
    }
}

/// Player N of `PlayerN`, 1 for `Handheld`; nothing for any other.
std::optional<int> playerOf(const Json& entry) {
    const Json& index = entry.value("player_index", Json{});
    if (isText(index, "Handheld")) {
        return 1;
    }
    for (int player = 1; player <= ryujinxPlayers; ++player) {
        if (isText(index, "Player" + std::to_string(player))) {
            return player;
        }
    }
    return std::nullopt;
}

Json stick(std::string_view side) {
    return Json{{"joystick", side},
                {"invert_stick_x", false},
                {"invert_stick_y", false},
                {"rotate90_cw", false},
                {"stick_button", std::string{side} + "Stick"}};
}

/// Ryujinx's own default for a pad of xpad's layout: Switch A east (SDL's B), B south.
Json defaultPad(std::string_view id, int player, const Json& controllerType) {
    return Json{
        {"left_joycon_stick", stick("Left")},
        {"right_joycon_stick", stick("Right")},
        {"deadzone_left", 0.1},
        {"deadzone_right", 0.1},
        {"range_left", 1},
        {"range_right", 1},
        {"trigger_threshold", 0.5},
        {"motion",
         {{"motion_backend", "GamepadDriver"},
          {"sensitivity", 100},
          {"gyro_deadzone", 1},
          {"enable_motion", true}}},
        {"rumble", {{"strong_rumble", 1}, {"weak_rumble", 1}, {"enable_rumble", false}}},
        {"led",
         {{"enable_led", false},
          {"turn_off_led", false},
          {"use_rainbow", false},
          {"led_color", 0}}},
        {"left_joycon",
         {{"button_minus", "Minus"},
          {"button_l", "LeftShoulder"},
          {"button_zl", "LeftTrigger"},
          {"button_sl", "SingleLeftTrigger0"},
          {"button_sr", "SingleRightTrigger0"},
          {"dpad_up", "DpadUp"},
          {"dpad_down", "DpadDown"},
          {"dpad_left", "DpadLeft"},
          {"dpad_right", "DpadRight"}}},
        {"right_joycon",
         {{"button_plus", "Plus"},
          {"button_r", "RightShoulder"},
          {"button_zr", "RightTrigger"},
          {"button_sl", "SingleLeftTrigger1"},
          {"button_sr", "SingleRightTrigger1"},
          {"button_x", "Y"},
          {"button_b", "A"},
          {"button_y", "X"},
          {"button_a", "B"}}},
        {"version", 1},
        {"backend", gamepadBackend},
        {"id", id},
        {"name", virtualPadSdlName},
        {"controller_type", controllerType.is_string() ? controllerType : Json("ProController")},
        {"player_index", "Player" + std::to_string(player)},
    };
}

void bindPlayers(Json& inputs, std::string_view padGuid) {
    bool hasPlayerOne = false;
    for (Json& entry : inputs) {
        const std::optional<int> player = entry.is_object() ? playerOf(entry) : std::nullopt;
        if (!player) {
            continue;
        }
        hasPlayerOne = hasPlayerOne || *player == 1;
        const std::string id = ryujinxPadId(padGuid, *player - 1);
        const Json backend = entry.value("backend", Json{});
        if (isText(backend, gamepadBackend)) {
            entry["id"] = id;
            lucent::info("ryujinx", "player {} bound to virtual pad {}", *player, id);
        } else if (isText(backend, keyboardBackend)) {
            entry = defaultPad(id, *player, entry.value("controller_type", Json{}));
            lucent::info("ryujinx", "player {} given a Pro Controller on virtual pad {}", *player,
                         id);
        }
    }
    if (!hasPlayerOne) {
        inputs.push_back(defaultPad(ryujinxPadId(padGuid, 0), 1, Json{}));
        lucent::info("ryujinx", "player 1 given a Pro Controller on virtual pad {}",
                     ryujinxPadId(padGuid, 0));
    }
}

} // namespace

std::string ryujinxPadId(std::string_view sdlGuid, int index) {
    if (sdlGuid.size() != 32) {
        return {};
    }
    // .NET reads the first 8 bytes as a little-endian int32 and two int16s.
    const auto byte = [sdlGuid](int at) {
        return sdlGuid.substr(static_cast<std::size_t>(at) * 2, 2);
    };
    std::string guid{"0000"};
    for (const int at : {1, 0}) {
        guid += byte(at);
    }
    guid += '-';
    for (const int at : {5, 4}) {
        guid += byte(at);
    }
    guid += '-';
    for (const int at : {7, 6}) {
        guid += byte(at);
    }
    guid += '-';
    guid += sdlGuid.substr(16, 4);
    guid += '-';
    guid += sdlGuid.substr(20);
    return std::to_string(index) + "-" + guid;
}

std::string ryujinxLaunchSettings(std::string_view json, std::string_view padGuid) {
    Json settings = Json::parse(json.empty() ? firstStartSettings : json, nullptr, false);
    if (!settings.is_object()) {
        lucent::warn("ryujinx", "the settings are not Ryujinx's JSON; they stay the player's");
        return std::string{json};
    }
    const Json version = settings.value("version", Json{});
    const int at = version.is_number_integer() ? version.get<int>() : 0;
    if (at > ryujinxNewestRead) {
        lucent::warn("ryujinx",
                     "settings version {} is newer than openSU reads; they stay the player's", at);
        return std::string{json};
    }
    if (at == ryujinxNewestRead) {
        // 71's one migration renamed the SDL2 audio backend to SDL3; undone, the file is 70.
        if (settings.contains("audio_backend")) {
            rename(settings["audio_backend"], audioRename);
        }
        settings["version"] = ryujinxSettingsVersion;
    }
    settings["update_checker_type"] = "Off";
    settings["check_updates_on_start"] = false;
    Json& inputs = settings["input_config"];
    if (!inputs.is_array()) {
        inputs = Json::array();
    }
    for (Json& entry : inputs) {
        if (entry.is_object() && entry.contains("backend")) {
            rename(entry["backend"], backendRename);
        }
    }
    if (!padGuid.empty()) {
        bindPlayers(inputs, padGuid);
    }
    return settings.dump(2);
}

std::vector<SettingsWrite> ryujinxWrites(const LaunchSetup& setup) {
    std::optional<SettingsWrite> run =
        settingsWriteOf(setup.config / ryujinxSettingsFile, [&setup](std::string_view player) {
            return ryujinxLaunchSettings(player, setup.padGuid);
        });
    if (!run) {
        return {};
    }
    return {std::move(*run)};
}

} // namespace opensu::library
