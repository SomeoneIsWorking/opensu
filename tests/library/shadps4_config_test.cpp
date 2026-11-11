// shadPS4's users.json as a launch reads it: a user for each of the four pad slots.
#include "library/shadps4_config.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "support/launch_writes.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using nlohmann::json;
using opensu::library::LaunchSetup;
using opensu::library::shadps4Id;
using opensu::library::shadps4LaunchUsers;
using opensu::library::shadps4UsersFile;
using opensu::test::expect;
using opensu::test::rewriteWritesNothing;
using opensu::test::writtenOver;

// One user on slot 1, as a player who removed shadPS4's others has it.
constexpr std::string_view player = R"({
  "Users": {
    "commit_hash": "dd18a5f29f2251ff3f0d5fad6a34ce262da0a40a",
    "user": [
      {"player_index": 1, "user_color": 1, "user_id": 1000, "user_name": "Player"}
    ]
  }
})";

void testSlots() {
    const json run = json::parse(shadps4LaunchUsers(player));
    const json& users = run["Users"]["user"];
    expect(users.size() == 4, "every pad slot has a user");
    expect(users[0]["user_name"] == "Player", "the player's user is kept as it is");
    for (int slot = 2; slot <= 4; ++slot) {
        const json& user = users[slot - 1];
        expect(user["player_index"] == slot, "the added user plays its slot");
        expect(user["user_name"] == "shadPS4-" + std::to_string(slot),
               "it is named as shadPS4 names its default users");
        expect(user["user_id"] == 999 + slot, "its id follows the player's");
    }
    expect(run["Users"]["commit_hash"] == "dd18a5f29f2251ff3f0d5fad6a34ce262da0a40a",
           "the rest of the file stays");
    const std::string full = shadps4LaunchUsers(player);
    expect(shadps4LaunchUsers(full) == full, "a list with every slot stays byte for byte");
}

void testUnread() {
    expect(shadps4LaunchUsers("").empty(), "without a file shadPS4 makes its own four users");
    expect(shadps4LaunchUsers("{not json") == "{not json", "a file that cannot be read stays");
    expect(shadps4LaunchUsers(R"({"Users": {}})") == R"({"Users": {}})",
           "a file without a user list gets shadPS4's defaults, so it stays");
}

void testWrites() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "shadps4-config"};
    const LaunchSetup setup{.emulatorId = std::string{shadps4Id},
                            .config = scratch.path(),
                            .data = {},
                            .padGuid = std::string{"0300cece5e0400008e02000010010000"}};
    expect(writtenOver(setup, scratch.path() / shadps4UsersFile, player) ==
               shadps4LaunchUsers(player),
           "shadPS4's user list are written in place");
    expect(rewriteWritesNothing(setup), "a second launch writes nothing");
}

} // namespace

int main() try {
    testSlots();
    testUnread();
    testWrites();
    std::printf("shadps4_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
