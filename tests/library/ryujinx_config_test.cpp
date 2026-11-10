// Ryujinx's Config.json as a launch reads it: the update check off, gamepad players on the virtual
// pads with their mappings, a Pro Controller for a keyboard player, SDL3 names as SDL2's.
#include "library/ryujinx_config.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using nlohmann::json;
using opensu::library::ryujinxLaunchSettings;
using opensu::library::ryujinxPadId;
using opensu::test::expect;

constexpr std::string_view sdlGuid = "0300cece5e0400008e02000010010000";

// The player's layout: player 1 on their own SDL3 pad, player 2 on the keyboard.
constexpr std::string_view player = R"json({
  "version": 70,
  "audio_backend": "SDL3",
  "check_updates_on_start": true,
  "update_checker_type": "PromptAtStartup",
  "input_config": [
    {
      "right_joycon": {"button_a": "B", "button_b": "A"},
      "version": 1,
      "backend": "GamepadSDL3",
      "id": "0-00000006-045e-0000-ea02-000008040000",
      "name": "Xbox One S Controller (0)",
      "controller_type": "ProController",
      "player_index": "Player1"
    },
    {
      "right_joycon": {"button_a": "Z"},
      "version": 1,
      "backend": "WindowKeyboard",
      "id": "0",
      "name": "Keyboard",
      "controller_type": "JoyconPair",
      "player_index": "Player2"
    }
  ]
})json";

void testPadId() {
    expect(ryujinxPadId(sdlGuid, 0) == "0-00000003-045e-0000-8e02-000010010000",
           "the GUID is .NET's byte order with the name's CRC zeroed");
    expect(ryujinxPadId(sdlGuid, 2).starts_with("2-"), "the index counts pads of one GUID");
}

void testUpdateCheck() {
    const json run = json::parse(ryujinxLaunchSettings(player, ""));
    expect(run["update_checker_type"] == "Off", "the update check is off");
    expect(run["check_updates_on_start"] == false, "an older config's check is off too");
    expect(run["audio_backend"] == "SDL2", "an SDL3 build's audio backend is written as SDL2");
    expect(run["input_config"][0]["id"] == "0-00000006-045e-0000-ea02-000008040000",
           "without virtual pads the players stay");
    expect(run["input_config"][0]["backend"] == "GamepadSDL2", "the gamepad backend is SDL2's");
}

void testPads() {
    const std::string text = ryujinxLaunchSettings(player, sdlGuid);
    const json run = json::parse(text);
    const json& one = run["input_config"][0];
    expect(one["id"] == ryujinxPadId(sdlGuid, 0), "player 1 plays the first virtual pad");
    expect(one["right_joycon"]["button_b"] == "A" && one["name"] == "Xbox One S Controller (0)",
           "the gamepad player's mappings are kept");
    const json& two = run["input_config"][1];
    expect(two["backend"] == "GamepadSDL2" && two["id"] == ryujinxPadId(sdlGuid, 1),
           "a keyboard player 2 plays the second virtual pad");
    expect(two["right_joycon"]["button_a"] == "B" && two["left_joycon"]["dpad_up"] == "DpadUp",
           "it gets the standard layout, Switch A on the east button");
    expect(two["controller_type"] == "JoyconPair" && two["player_index"] == "Player2",
           "its controller type and player are kept");
    expect(ryujinxLaunchSettings(text, sdlGuid) == text, "a file already set stays as it is");

    const json lone = json::parse(ryujinxLaunchSettings(R"({"input_config": []})", sdlGuid));
    expect(lone["input_config"].size() == 1 &&
               lone["input_config"][0]["player_index"] == "Player1" &&
               lone["input_config"][0]["id"] == ryujinxPadId(sdlGuid, 0),
           "player 1 without an entry gets the first virtual pad");
    expect(ryujinxLaunchSettings("not json", sdlGuid) == "not json",
           "a file that is not Ryujinx's JSON is left alone");
}

} // namespace

int main() try {
    testPadId();
    testUpdateCheck();
    testPads();
    std::printf("ryujinx_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
