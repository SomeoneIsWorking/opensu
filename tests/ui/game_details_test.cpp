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

std::vector<std::string> labelsOf(const std::vector<SettingsRow>& rows) {
    std::vector<std::string> labels;
    labels.reserve(rows.size());
    for (const SettingsRow& row : rows) {
        labels.push_back(row.label);
    }
    return labels;
}

std::vector<std::string> idsOf(const GameDetails& details) {
    std::vector<std::string> ids;
    ids.reserve(details.actions.size());
    for (const SettingsRow& row : details.actions) {
        ids.push_back(row.id);
    }
    return ids;
}

/// What `game`'s Game settings page holds besides its settings.
std::vector<SettingsRow> settingsPageRows(const Game& game, bool ownWinePrefix) {
    return settingsPageOf(detailsOf(game, GameMarks{.hidden = false}, now, {}, ownWinePrefix),
                          game.id);
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
        EmulatorOption{
            "Eden", edenInstalled, LaunchSpec{"eden", {}}, "eden", false, {}, {}, {}, {}, false},
        EmulatorOption{"Ryujinx", false, {}, "ryujinx", false, {}, {}, {}, {}, false}};
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

void ownedInTwoStores() {
    Game steam = hades();
    steam.updateAvailable = true;
    Game epic = hades();
    epic.id = "epic:hades";
    epic.source = Source::Epic;
    epic.installed = false;
    epic.lastPlayed.reset();
    const std::vector<CopyPart> copies{{steam, false, {}}, {epic, false, {}}};
    const GameDetails details =
        detailsOf(steam, GameMarks{.hidden = false}, now, {}, false, {}, copies);
    expect(details.launch.label == "Play (Steam)", "the top copy's Play leads");
    expect(labelsOf(details.actions) ==
               std::vector<std::string>{"Update (Steam)", "Add to Home", "Hide game",
                                        "Game settings (Steam)", "More", "Back"},
           "the page's rows are the top copy's");
    expect(labelsOf(details.more) == std::vector<std::string>{"Game settings (Epic)",
                                                              "Install (Epic)",
                                                              "Uninstall (Steam)"},
           "More has the other copy's rows, then what removes each");
    const CopyRow install = copyRowOf(details.more[1].id);
    expect(install.gameId == "epic:hades" && install.id == detailsInstallId,
           "a row names its copy");
    expect(copyRowOf(std::string{detailsBackId}).gameId.empty(), "a row of the page names none");
    Game away = epic;
    away.source = Source::Steam;
    away.id = "steam:other";
    const std::vector<CopyPart> uninstalled{{away, false, {}}, {epic, false, {}}};
    const GameDetails nothing =
        detailsOf(away, GameMarks{.hidden = false}, now, {}, false, {}, uninstalled);
    expect(nothing.launch.label == "Install (Steam)" &&
               labelsOf(nothing.more) ==
                   std::vector<std::string>{"Game settings (Epic)", "Install (Epic)"},
           "with nothing installed the top Install leads and is not on More again");
    expect(valueOf(details, "Last played (Epic)") == "Never played" &&
               valueOf(details, "Last played (Steam)") == "3 hours ago" &&
               factOf(details, "Last played") == nullptr,
           "a fact the copies differ on says whose it is");
    Game both = epic;
    both.installed = true;
    both.lastPlayed = hades().lastPlayed;
    both.playtimeMinutes = hades().playtimeMinutes;
    const std::vector<CopyPart> alike{{hades(), false, {}}, {both, false, {}}};
    expect(valueOf(detailsOf(hades(), GameMarks{.hidden = false}, now, {}, false, {}, alike),
                   "Last played") == "3 hours ago",
           "one they read alike shows once");
}

void storeGame() {
    const GameDetails details = detailsOf(hades(), GameMarks{.hidden = false, .onHome = true}, now);
    expect(details.title == "Hades" && details.badge == "Steam", "title and store badge");
    expect(details.art == "/covers/hades.jpg", "the cover is the game's own");
    expect(valueOf(details, "Status") == "Installed", "installed state");
    expect(valueOf(details, "Last played") == "3 hours ago", "last played");
    expect(valueOf(details, "Play time") == "2 h 5 min", "play time");
    expect(factOf(details, "Emulator") == nullptr, "a store game has no emulator fact");
    expect(details.launch.label == "Play", "an installed game plays");
    GameSettingsView seated;
    seated.continues = true;
    expect(detailsOf(hades(), GameMarks{.hidden = false}, now, {}, false, seated).launch.label ==
               "Continue here",
           "a game the player's profile is in continues");
    expect(idsOf(details) == ids({detailsHomeId, detailsHiddenId, detailsSettingsId, detailsMoreId,
                                  detailsBackId}),
           "Remove from Home, Hide, Game settings, More, Back");
    expect(labelsOf(details.more) == std::vector<std::string>{"Uninstall"},
           "a one-store game's Uninstall is on More");
    expect(actionOf(details, detailsHomeId)->label == "Remove from Home",
           "a game on Home offers to be taken off it");
    const std::vector<SettingsRow> more = settingsPageRows(hades(), false);
    expect(actionOf(details, detailsHiddenId)->label == "Hide game" && more.size() == 3 &&
               copyRowOf(more[0].id).id == detailsMatchId &&
               copyRowOf(more[1].id).id == detailsArtworkId &&
               copyRowOf(more[2].id).id == detailsControlsId,
           "Hide acts; Match, Choose artwork and Controls are on the Game settings page");
    Game wide = hades();
    wide.artworkWide = "/covers/hades-wide.jpg";
    expect(detailsOf(wide, GameMarks{.hidden = false}, now).hero == "/covers/hades-wide.jpg",
           "beside a cover the wide picture is the backdrop");
    expect(details.hero.empty(), "no wide picture, no backdrop");
    wide.artwork.clear();
    expect(detailsOf(wide, GameMarks{.hidden = false}, now).art == "/covers/hades-wide.jpg" &&
               detailsOf(wide, GameMarks{.hidden = false}, now).hero.empty(),
           "without a cover the wide picture shows, and not twice");
}

void states() {
    Game missing = hades();
    missing.installed = false;
    const GameDetails details = detailsOf(missing, GameMarks{.hidden = true}, now);
    expect(details.launch.label == "Install", "a game that is not installed offers Install");
    expect(valueOf(details, "Status") == "Not installed, hidden", "hidden shows");
    expect(actionOf(details, detailsHiddenId)->label == "Show game",
           "a hidden game offers to be shown");
    expect(actionOf(details, detailsHomeId)->label == "Add to Home",
           "a game off Home offers to be added");
    expect(actionOf(details, detailsUninstallId) == nullptr,
           "a game that is not installed has nothing to uninstall");
    Game updatable = hades();
    updatable.updateAvailable = true;
    const GameDetails update = detailsOf(updatable, GameMarks{.hidden = false}, now);
    expect(update.actions.front().id == detailsUpdateId &&
               valueOf(update, "Status") == "Installed, update available",
           "an update follows Play, and the status says so");
}

void romEmulator() {
    const GameDetails details = detailsOf(kirby(true), GameMarks{.hidden = false}, now);
    expect(details.badge == "Nintendo Switch" || details.badge == "Switch",
           "a ROM's badge is its system");
    expect(valueOf(details, "Emulator") == "Eden · installed",
           "the emulator fact names it and says it is installed");
    expect(details.more.empty() && actionOf(details, detailsMoreId) == nullptr,
           "a ROM is never uninstalled, which leaves More out");
    const GameDetails missing = detailsOf(kirby(false), GameMarks{.hidden = false}, now);
    expect(valueOf(missing, "Emulator") == "Eden · not installed",
           "an emulator that is not installed says so");
    Game unpicked = kirby(true);
    unpicked.emulator.clear();
    const GameDetails unpickedDetails = detailsOf(unpicked, GameMarks{.hidden = false}, now);
    const GameDetails sourced = detailsOf(kirby(true), GameMarks{.hidden = false}, now, {}, false,
                                          GameSettingsView{"Set for this game",
                                                           {},
                                                           "Inherited",
                                                           "Eden, Ryujinx",
                                                           {},
                                                           {},
                                                           {},
                                                           false,
                                                           false,
                                                           {},
                                                           false});
    expect(valueOf(sourced, "Emulator") == "Eden · installed · Set for this game" &&
               actionOf(sourced, detailsSettingsId)->value == "Inherited",
           "where the pick comes from shows, and the Game settings row reads its count");
    const SettingsRow* saves = settingsRowOf(sourced, kirby(true).id, detailsSavesId);
    expect(saves != nullptr && saves->kind == RowKind::Page && saves->value == "Eden, Ryujinx" &&
               settingsRowOf(unpickedDetails, unpicked.id, detailsSavesId) == nullptr,
           "Copy saves names the emulators keeping a save, and is left out while none is named");
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
                                           "lacks 1 of 16 files: avsp.key",
                                           {},
                                           false},
                            EmulatorOption{"MAME 2010",
                                           false,
                                           {},
                                           "mame2010",
                                           true,
                                           {},
                                           opensu::library::RomsetFit::Mismatch,
                                           "lacks 2 of 15 files: a, b",
                                           {},
                                           false}};
    avsp.romset = "No arcade core matches this romset";
    avsp.romsetNearest = "MAME 2010 lacks 2 of 15 files: a, b";
    avsp.unavailable = avsp.romset;
    const GameDetails none = detailsOf(avsp, GameMarks{.hidden = false}, now);
    expect(valueOf(none, "Romset") == "No arcade core matches this romset" &&
               valueOf(none, "Nearest") == "MAME 2010 lacks 2 of 15 files: a, b" &&
               valueOf(none, "Emulator") == "none for this romset",
           "a romset no core matches says so and names the nearest core's gap");
    expect(emulatorToDownload(avsp) == nullptr,
           "a core the romset does not match is not offered for download");
    avsp.emulator = "FinalBurn Neo";
    const GameDetails picked = detailsOf(avsp, GameMarks{.hidden = false}, now, "42% · A cancels");
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
    expect(valueOf(detailsOf(avsp, GameMarks{.hidden = false}, now), "Emulator") ==
               "MAME 2010 · not installed · romset matches",
           "the emulator fact says the romset matches");
}

void winePrefix() {
    Game windows = hades();
    windows.id = "gog:2";
    windows.source = Source::Gog;
    windows.windowsInstall = true;
    const SettingsRow shared = settingsPageRows(windows, false).front();
    expect(shared.label == "Wine prefix" && shared.value == sharedPrefix &&
               copyRowOf(shared.id).gameId == "gog:2",
           "a Windows install shows its prefix on Game settings, shared by default");
    expect(settingsPageRows(windows, true).front().value == ownPrefix,
           "a game with its own prefix says so");
    expect(settingsPageRows(hades(), false).front().label != "Wine prefix",
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
    const GameDetails details = detailsOf(game, GameMarks{.hidden = false}, now);
    expect(details.badge == "Your games" && valueOf(details, "File") == game.file.string(),
           "an added game says it is the player's and which file it starts");
    expect(idsOf(details) == ids({detailsHomeId, detailsHiddenId, detailsSettingsId, detailsMoreId,
                                  detailsBackId}),
           "the same page as a store game's");
    expect(labelsOf(details.more) == std::vector<std::string>{"Remove from library"},
           "with Remove from library where Uninstall stands");
    expect(settingsRowOf(details, game.id, detailsRenameId) != nullptr &&
               settingsRowOf(details, game.id, detailsChangeFileId) != nullptr,
           "rename and change file on Game settings");
    expect(settingsPageRows(game, false).front().label != "Wine prefix",
           "and no Wine prefix: it has its own");
    expect(valueOf(details, "Steam app") == "None matched", "no Steam app is said");

    game.file.clear();
    const GameDetails lutris = detailsOf(game, GameMarks{.hidden = false}, now);
    game.artworkKey = "2050650";
    expect(valueOf(detailsOf(game, GameMarks{.hidden = false}, now), "Steam app") == "2050650",
           "the Steam app it is matched to is said");
    expect(valueOf(lutris, "Runs through") == "Lutris" &&
               settingsRowOf(lutris, game.id, detailsChangeFileId) == nullptr,
           "a Lutris game has no file to change");
}

} // namespace

int main() try {
    ownGame();
    lastPlayed();
    storeGame();
    ownedInTwoStores();
    states();
    romEmulator();
    arcadeRomset();
    winePrefix();
    std::puts("game_details: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
