// What a game's details page says: its facts, its cover and its launch button and action rows.
#include "game_details.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using namespace opensu::ui;
using namespace std::chrono;
using opensu::library::EmulatorOption;
using opensu::library::Game;
using opensu::library::LaunchSpec;
using opensu::library::Source;
using opensu::test::expect;

const system_clock::time_point now = system_clock::from_time_t(1'800'000'000);

std::string valueOf(const GameDetails& details, const char* label) {
    const SettingsRow* fact = factOf(details, label);
    return fact == nullptr ? std::string{} : fact->value;
}

std::vector<std::string> idsOf(const GameDetails& details) {
    std::vector<std::string> ids;
    ids.reserve(details.actions.size());
    for (const SettingsRow& row : details.actions) {
        ids.push_back(row.id);
    }
    return ids;
}

std::vector<std::string> ids(std::initializer_list<std::string_view> names) {
    return {names.begin(), names.end()};
}

Game hades() {
    Game game;
    game.id = "steam:1145360";
    game.source = Source::Steam;
    game.title = "Hades";
    game.installed = true;
    game.playtimeMinutes = 125;
    game.lastPlayed = now - hours{3};
    game.artwork = "/covers/hades.jpg";
    return game;
}

Game kirby(bool edenInstalled) {
    Game game;
    game.id = "rom:/r/Kirby.nsp";
    game.source = Source::Rom;
    game.sourceId = "switch";
    game.title = "Kirby";
    game.installed = true;
    game.emulatorOptions = {
        EmulatorOption{"Eden", edenInstalled, LaunchSpec{"eden", {}}, "eden", false, {}, {}, {}},
        EmulatorOption{"Ryujinx", false, {}, "ryujinx", false, {}, {}, {}}};
    game.emulator = "Eden";
    return game;
}

void lastPlayed() {
    expect(lastPlayedText(std::nullopt, now) == "Never played", "no launch reads never played");
    expect(lastPlayedText(now - seconds{20}, now) == "Just now", "seconds ago is just now");
    expect(lastPlayedText(now - hours{3}, now) == "3 hours ago", "hours");
    expect(lastPlayedText(now - hours{24}, now) == "1 day ago", "a day is singular");
    expect(lastPlayedText(now - hours{24 * 90}, now).size() == 10, "months back is a date");
}

void storeGame() {
    const GameDetails details = detailsOf(hades(), false, now);
    expect(details.title == "Hades" && details.badge == "Steam", "title and store badge");
    expect(details.art == "/covers/hades.jpg", "the cover is the game's own");
    expect(valueOf(details, "Status") == "Installed", "installed state");
    expect(valueOf(details, "Last played") == "3 hours ago", "last played");
    expect(valueOf(details, "Play time") == "2 h 5 min", "play time");
    expect(factOf(details, "Emulator") == nullptr, "a store game has no emulator fact");
    expect(details.launch == "Play", "an installed game plays");
    expect(idsOf(details) == ids({detailsHiddenId, detailsUninstallId, detailsArtworkId,
                                  detailsControlsId, detailsBackId}),
           "Hide, Uninstall, Choose artwork, Controls, Back");
    expect(actionOf(details, detailsHiddenId)->label == "Hide game" &&
               actionOf(details, detailsArtworkId)->kind == RowKind::Page &&
               actionOf(details, detailsControlsId)->kind == RowKind::Page,
           "Hide acts; Choose artwork and Controls open what is over the page");
    Game wide = hades();
    wide.artworkWide = "/covers/hades-wide.jpg";
    expect(detailsOf(wide, false, now).hero == "/covers/hades-wide.jpg",
           "beside a cover the wide picture is the backdrop");
    expect(details.hero.empty(), "no wide picture, no backdrop");
    wide.artwork.clear();
    expect(detailsOf(wide, false, now).art == "/covers/hades-wide.jpg" &&
               detailsOf(wide, false, now).hero.empty(),
           "without a cover the wide picture shows, and not twice");
}

void states() {
    Game missing = hades();
    missing.installed = false;
    const GameDetails details = detailsOf(missing, true, now);
    expect(details.launch == "Install", "a game that is not installed offers Install");
    expect(valueOf(details, "Status") == "Not installed, hidden", "hidden shows");
    expect(actionOf(details, detailsHiddenId)->label == "Show game",
           "a hidden game offers to be shown");
    expect(actionOf(details, detailsUninstallId) == nullptr,
           "a game that is not installed has nothing to uninstall");
    Game updatable = hades();
    updatable.updateAvailable = true;
    const GameDetails update = detailsOf(updatable, false, now);
    expect(update.actions.front().id == detailsUpdateId &&
               valueOf(update, "Status") == "Installed, update available",
           "an update follows Play, and the status says so");
}

void romEmulator() {
    const GameDetails details = detailsOf(kirby(true), false, now);
    expect(details.badge == "Nintendo Switch" || details.badge == "Switch",
           "a ROM's badge is its system");
    expect(valueOf(details, "Emulator") == "Eden · installed",
           "the emulator fact names it and says it is installed");
    const SettingsRow* emulator = actionOf(details, detailsEmulatorId);
    expect(emulator != nullptr && emulator->kind == RowKind::Choice &&
               emulator->choices == std::vector<std::string>{"Eden", "Ryujinx"} &&
               emulator->chosen == 0 && details.actions.front().id == detailsEmulatorId,
           "a ROM with two emulators chooses between them right after Play");
    expect(actionOf(details, detailsUninstallId) == nullptr, "a ROM is never uninstalled");
    const GameDetails missing = detailsOf(kirby(false), false, now);
    expect(valueOf(missing, "Emulator") == "Eden · not installed",
           "an emulator that is not installed says so");
    Game single = kirby(true);
    single.emulatorOptions.pop_back();
    expect(actionOf(detailsOf(single, false, now), detailsEmulatorId) == nullptr,
           "one emulator leaves nothing to choose");
    Game unpicked = kirby(true);
    unpicked.emulator.clear();
    const GameDetails unpickedDetails = detailsOf(unpicked, false, now);
    const SettingsRow* none = actionOf(unpickedDetails, detailsEmulatorId);
    expect(none != nullptr && none->choices.front() == noEmulator && none->chosen == 0 &&
               none->choices.size() == 3,
           "with none picked the choice says so before the emulators");
    const GameDetails sourced = detailsOf(kirby(true), false, now, {}, false,
                                          GameSettingsView{"Set for this game", {}, "Inherited"});
    expect(valueOf(sourced, "Emulator") == "Eden · installed · Set for this game" &&
               actionOf(sourced, detailsEmulatorId)->note == "Set for this game" &&
               actionOf(sourced, detailsSettingsId)->value == "Inherited",
           "where the pick comes from shows, and the Game settings row reads its count");
}

void arcadeRomset() {
    Game avsp;
    avsp.id = "rom:/r/Arcade/avsp.zip";
    avsp.source = Source::Rom;
    avsp.sourceId = "arcade";
    avsp.installed = true;
    avsp.emulatorOptions = {EmulatorOption{"FinalBurn Neo",
                                           false,
                                           {},
                                           "fbneo",
                                           true,
                                           {},
                                           opensu::library::RomsetFit::Mismatch,
                                           "lacks 1 of 16 files: avsp.key"},
                            EmulatorOption{"MAME 2010",
                                           false,
                                           {},
                                           "mame2010",
                                           true,
                                           {},
                                           opensu::library::RomsetFit::Mismatch,
                                           "lacks 2 of 15 files: a, b"}};
    avsp.romset = "No arcade core matches this romset";
    avsp.romsetNearest = "MAME 2010 lacks 2 of 15 files: a, b";
    avsp.unavailable = avsp.romset;
    const GameDetails none = detailsOf(avsp, false, now);
    expect(valueOf(none, "Romset") == "No arcade core matches this romset" &&
               valueOf(none, "Nearest") == "MAME 2010 lacks 2 of 15 files: a, b" &&
               valueOf(none, "Emulator") == "none for this romset",
           "a romset no core matches says so and names the nearest core's gap");
    expect(emulatorToDownload(avsp) == nullptr,
           "a core the romset does not match is not offered for download");
    avsp.emulator = "FinalBurn Neo";
    const GameDetails picked = detailsOf(avsp, false, now, "42% · A cancels");
    expect(emulatorToDownload(avsp) == &avsp.emulatorOptions[0] &&
               valueOf(picked, "Emulator") ==
                   "FinalBurn Neo · not installed · romset does not match",
           "a core the player picked is offered even when the romset does not match");
    expect(picked.actions.front().id == detailsDownloadId &&
               picked.actions.front().label == "Download FinalBurn Neo" &&
               picked.actions.front().value == "42% · A cancels",
           "the download follows Play and reads its progress");
    avsp.emulatorOptions[1].romset = opensu::library::RomsetFit::Matches;
    avsp.emulator = "MAME 2010";
    expect(valueOf(detailsOf(avsp, false, now), "Emulator") ==
               "MAME 2010 · not installed · romset matches",
           "the emulator fact says the romset matches");
}

void winePrefix() {
    Game windows = hades();
    windows.id = "gog:2";
    windows.source = Source::Gog;
    windows.windowsInstall = true;
    const GameDetails windowsDetails = detailsOf(windows, false, now);
    const SettingsRow* shared = actionOf(windowsDetails, detailsWinePrefixId);
    expect(shared != nullptr && shared->label == "Wine prefix" && shared->value == sharedPrefix,
           "a Windows install shows its prefix, shared by default");
    expect(actionOf(detailsOf(windows, false, now, {}, true), detailsWinePrefixId)->value ==
               ownPrefix,
           "a game with its own prefix says so");
    expect(actionOf(detailsOf(hades(), false, now), detailsWinePrefixId) == nullptr,
           "a game that does not run under Wine has no prefix row");
}

void ownGame() {
    Game game;
    game.id = "own:3";
    game.source = Source::Own;
    game.sourceId = "3";
    game.title = "Celeste";
    game.file = "/games/Celeste/Celeste.exe";
    game.installed = true;
    game.windowsInstall = true;
    const GameDetails details = detailsOf(game, false, now);
    expect(details.badge == "Your games" && valueOf(details, "File") == game.file.string(),
           "an added game says it is the player's and which file it starts");
    expect(idsOf(details) ==
               ids({detailsHiddenId, detailsRenameId, detailsChangeFileId, detailsRemoveId,
                    detailsArtworkId, detailsControlsId, detailsBackId}),
           "rename, change file and remove; no Uninstall, and no Wine prefix: it has its own");

    game.file.clear();
    const GameDetails lutris = detailsOf(game, false, now);
    expect(valueOf(lutris, "Runs through") == "Lutris" &&
               actionOf(lutris, detailsChangeFileId) == nullptr,
           "a Lutris game has no file to change");
}

} // namespace

int main() try {
    ownGame();
    lastPlayed();
    storeGame();
    states();
    romEmulator();
    arcadeRomset();
    winePrefix();
    std::puts("game_details: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
