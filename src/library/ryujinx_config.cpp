#include "ryujinx_config.hpp"

#include <array>
#include <optional>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/read_whole.hpp"
#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

constexpr std::string_view gamepadBackend{"GamepadSDL2"};
constexpr std::string_view keyboardBackend{"WindowKeyboard"};
constexpr std::string_view virtualPadName{"Xbox 360 Controller"};

/// An SDL3 build's enum name and the SDL2 one every build reads.
struct Rename {
    std::string_view sdl3;
    std::string_view sdl2;
};

constexpr Rename audioRename{.sdl3 = "SDL3", .sdl2 = "SDL2"};
constexpr Rename backendRename{.sdl3 = "GamepadSDL3", .sdl2 = gamepadBackend};

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
         {{"enable_led", false}, {"turn_off_led", false}, {"use_rainbow", false}, {"led_color", 0}}},
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
        {"name", virtualPadName},
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
    const auto byte = [sdlGuid](int at) { return sdlGuid.substr(static_cast<std::size_t>(at) * 2, 2); };
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
    Json settings = Json::parse(json, nullptr, false);
    if (!settings.is_object()) {
        lucent::warn("ryujinx", "the settings are not Ryujinx's JSON; they stay the player's");
        return std::string{json};
    }
    settings["update_checker_type"] = "Off";
    settings["check_updates_on_start"] = false;
    if (settings.contains("audio_backend")) {
        rename(settings["audio_backend"], audioRename);
    }
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

std::vector<fs::path> ryujinxFiles(const LaunchSetup& setup) {
    return {setup.config / ryujinxSettingsFile};
}

std::vector<RunFile> ryujinxRunFiles(const LaunchSetup& setup) {
    const fs::path file = setup.config / ryujinxSettingsFile;
    std::error_code missing;
    if (!fs::exists(file, missing)) {
        lucent::info("ryujinx", "{} is not there yet; Ryujinx writes it on its first start",
                     file.string());
        return {};
    }
    std::string error;
    std::optional<std::string> player = fileio::readWhole(file, error);
    if (!player) {
        lucent::warn("ryujinx", "{} stays the player's: {}", file.string(), error);
        return {};
    }
    std::string run = ryujinxLaunchSettings(*player, setup.padGuid);
    if (run == *player) {
        return {};
    }
    return {RunFile{.path = file, .bytes = std::move(run)}};
}

} // namespace opensu::library
