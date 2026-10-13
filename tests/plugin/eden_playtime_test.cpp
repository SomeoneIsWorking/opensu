// The bundled Eden plugin, run from the shipped assets through the shipping host against
// fixture Eden files: play time from `playtime.bin` for games found by the program id in their
// file name (an update's id too) or by their title in Eden's game list cache, a half-written file
// giving no new answer, and the play time merged into the library once per session.
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "library/play_time.hpp"
#include "plugin_rig.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using test::expect;

/// `records` as Eden writes them: { u64 program id, u64 seconds }, little-endian.
std::string playtimeBin(const std::vector<std::pair<std::uint64_t, std::uint64_t>>& records) {
    std::string bytes;
    const auto put = [&bytes](std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) {
            bytes.push_back(static_cast<char>((value >> shift) & 0xFFU));
        }
    };
    for (const auto& [id, seconds] : records) {
        put(id);
        put(seconds);
    }
    return bytes;
}

} // namespace

int main() {
    test::PluginRig rig{fs::path{OPENSU_TEST_SCRATCH} / "eden_playtime"};
    const fs::path playTime = rig.root() / "data" / "eden" / "play_time" / "playtime.bin";
    const fs::path gameList = rig.root() / "cache" / "eden" / "game_list";
    test::writeFile(playTime, playtimeBin({{0x0100000000010000, 17145},
                                           {0x0100C1F0051B6000, 2700},
                                           {0x010093801237C000, 118},
                                           {0x0100E95004038000, 5000},
                                           {0, 999},
                                           {0x0100152000022000, 6067}}));
    test::writeFile(gameList / "0100000000010000.pv.txt", "Update (1.3.0)");
    test::writeFile(gameList / "010093801237C000.pv.txt", "");
    test::writeFile(gameList / "010093801237C000.appname.txt", "Metroid Dread");
    test::writeFile(gameList / "0100152000022000.pv.txt", "Update (3.0.3)\nDLC");

    const fs::path switchDir = "/roms/Switch";
    const library::Game odyssey =
        test::romGame("Super Mario Odyssey", switchDir / "Super Mario Odyssey" /
                                                 "Super Mario Odyssey [0100000000010000][v0].nsp");
    const library::Game freeze =
        test::romGame("Donkey Kong Country Tropical Freeze",
                      switchDir / "Donkey Kong Country Tropical Freeze [0100C1F0051B6000][v0].nsp");
    const library::Game dread =
        test::romGame("Metroid Dread", switchDir / "Metroid Dread" / "Metroid Dread.nsp");
    const library::Game blades = test::romGame(
        "Xenoblade Chronicles 2",
        switchDir / "Xenoblade Chronicles 2" / "Xenoblade Chronicles 2 [0100E95004038800][v9].nsp");
    const library::Game kart =
        test::romGame("Mario Kart 8 Deluxe", switchDir / "Mario Kart 8 Deluxe" / "MK8.nsp");
    const library::Game ps2 = test::romGame("Odyssey", "/roms/ps2/[0100000000010000].iso", "ps2");
    std::vector<library::Game> games{odyssey, freeze, dread, blades, kart, ps2};

    rig.start(OPENSU_ASSETS_DIR "/plugins");
    const plugin::PluginInfo& eden = rig.info("eden-playtime");
    expect(eden.bundled && eden.state == plugin::PluginInfo::State::Running,
           "the bundled plugin loads from the assets");

    plugin::PluginHost::Update update = rig.refresh(games);
    const library::PlayTimeReports::Report& report = update.playTime.at("eden-playtime");
    expect(report.size() == 4, "four games have Eden play time");
    expect(report.at(odyssey.id) == 17145, "a program id in the file name finds the game");
    expect(report.at(freeze.id) == 2700, "a file at the top of the system folder too");
    expect(report.at(blades.id) == 5000, "an update's id maps to its game's");
    expect(report.at(dread.id) == 118, "an untagged game is found by its title in Eden's cache");
    expect(!report.contains(kart.id), "a game Eden's cache names no title for is not guessed");
    expect(!report.contains(ps2.id), "only the systems the plugin claims are given to it");
    expect(eden.listed == 3 && eden.matched == 1,
           "the game list holds Eden's three games; one matched openSU's by title");

    library::PlayTimeReports reports;
    reports.set("eden-playtime", report);
    reports.apply(games);
    expect(games[0].playtimeMinutes == 285, "17145 s is 4 h 45 min");

    // Eden caught mid-write: a size that is not whole records.
    test::writeFile(playTime, playtimeBin({{0x0100000000010000, 17175}}) + "half");
    update = rig.refresh(games);
    expect(!update.playTime.contains("eden-playtime") && eden.faults == 0,
           "a half-written file is no new answer and no fault");
    reports.apply(games);
    expect(games[0].playtimeMinutes == 285, "the last good answer stays in use");

    test::writeFile(playTime, playtimeBin({{0x0100000000010000, 17205}}));
    update = rig.refresh(games);
    reports.set("eden-playtime", update.playTime.at("eden-playtime"));
    reports.apply(games);
    expect(games[0].playtimeMinutes == 286 && games[1].playtimeMinutes == 0,
           "the next whole file replaces the answer");

    fs::remove_all(rig.root());
    std::printf("eden_playtime: all checks passed\n");
    return 0;
}
