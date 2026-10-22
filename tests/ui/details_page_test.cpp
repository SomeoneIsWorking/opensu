// The details page: what it says about a game, its buttons, its geometry, its focus and the armed
// Uninstall.
#include "details_page.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using namespace std::chrono;
using opensu::library::EmulatorOption;
using opensu::library::Game;
using opensu::library::LaunchSpec;
using opensu::library::Source;
using opensu::test::expect;
using opensu::test::need;

const system_clock::time_point now = system_clock::from_time_t(1'800'000'000);

std::string valueOf(const DetailsView& view, const char* label) {
    for (const DetailsRow& candidate : view.rows) {
        if (candidate.label == label) {
            return candidate.value;
        }
    }
    return {};
}

Game hades() {
    Game game;
    game.id = "steam:1145360";
    game.source = Source::Steam;
    game.title = "Hades";
    game.installed = true;
    game.playtimeMinutes = 125;
    game.lastPlayed = now - hours{3};
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
    const DetailsView view = detailsFor(hades(), false, now);
    expect(view.title == "Hades" && view.badge == "Steam", "title and store badge");
    expect(valueOf(view, "Status") == "Installed", "installed state");
    expect(valueOf(view, "Last played") == "3 hours ago", "last played");
    expect(valueOf(view, "Play time") == "2 h 5 min", "play time");
    expect(valueOf(view, "Emulator").empty(), "a store game has no emulator row");
    expect(view.buttons.size() == 6 && view.buttons[0].label == "Play" &&
               view.buttons[1].label == "Hide game" &&
               view.buttons[2].action == DetailsAction::Uninstall &&
               view.buttons[2].label == "Uninstall" &&
               view.buttons[3].action == DetailsAction::ChooseArtwork &&
               view.buttons[3].label == "Choose artwork" &&
               view.buttons[4].action == DetailsAction::Controls &&
               view.buttons[4].label == "Controls" && view.buttons[5].action == DetailsAction::Back,
           "Play, Hide, Uninstall, Choose artwork, Controls, Back");
}

void states() {
    Game missing = hades();
    missing.installed = false;
    const DetailsView view = detailsFor(missing, true, now);
    expect(view.buttons[0].label == "Install", "a game that is not installed offers Install");
    expect(valueOf(view, "Status") == "Not installed, hidden", "hidden shows");
    expect(view.buttons[1].label == "Show game", "a hidden game offers to be shown");
    expect(view.buttons.size() == 5 && view.buttons[3].action == DetailsAction::Controls &&
               view.buttons[4].action == DetailsAction::Back,
           "a game that is not installed has nothing to uninstall");
}

void romEmulator() {
    const DetailsView view = detailsFor(kirby(true), false, now);
    expect(view.badge == "Nintendo Switch" || view.badge == "Switch",
           "a ROM's badge is its system");
    expect(valueOf(view, "Emulator") == "Eden · installed",
           "the emulator row names it and says it is installed");
    expect(view.buttons[1].action == DetailsAction::Emulator && view.buttons[1].value == "Eden",
           "a ROM with two emulators has a button to change it");
    const DetailsView missing = detailsFor(kirby(false), false, now);
    expect(valueOf(missing, "Emulator") == "Eden · not installed",
           "an emulator that is not installed says so");
    Game single = kirby(true);
    single.emulatorOptions.pop_back();
    expect(detailsFor(single, false, now).buttons.size() == 5,
           "one emulator leaves nothing to choose");
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
    const DetailsView none = detailsFor(avsp, false, now);
    expect(valueOf(none, "Romset") == "No arcade core matches this romset" &&
               valueOf(none, "Nearest") == "MAME 2010 lacks 2 of 15 files: a, b" &&
               valueOf(none, "Emulator") == "none for this romset",
           "a romset no core matches says so and names the nearest core's gap");
    expect(emulatorToDownload(avsp) == nullptr,
           "a core the romset does not match is not offered for download");
    avsp.emulator = "FinalBurn Neo";
    expect(emulatorToDownload(avsp) == &avsp.emulatorOptions[0] &&
               valueOf(detailsFor(avsp, false, now), "Emulator") ==
                   "FinalBurn Neo · not installed · romset does not match",
           "a core the player picked is offered even when the romset does not match");
    avsp.emulatorOptions[1].romset = opensu::library::RomsetFit::Matches;
    avsp.emulator = "MAME 2010";
    expect(valueOf(detailsFor(avsp, false, now), "Emulator") ==
               "MAME 2010 · not installed · romset matches",
           "the emulator row says the romset matches");
}

void winePrefix() {
    Game windows = hades();
    windows.id = "gog:2";
    windows.source = Source::Gog;
    windows.windowsInstall = true;
    const DetailsView shared = detailsFor(windows, false, now);
    expect(shared.buttons[1].action == DetailsAction::WinePrefix &&
               shared.buttons[1].label == "Wine prefix" && shared.buttons[1].value == "Shared",
           "a Windows install shows its prefix, shared by default");
    expect(detailsFor(windows, false, now, {}, true).buttons[1].value == "Own",
           "a game with its own prefix says so");
    expect(std::ranges::none_of(detailsFor(hades(), false, now).buttons,
                                [](const DetailsButton& button) {
                                    return button.action == DetailsAction::WinePrefix;
                                }),
           "a game that does not run under Wine has no prefix button");
}

void layout() {
    const DetailsLayout page =
        layoutDetails(1280.0f, 800.0f, 1.5f, 90.0f, 70.0f, DetailsParts{140.0f, 3});
    expect(page.buttons.size() == 3 && page.art.y == 90.0f && page.art.bottom() == 730.0f,
           "the cover fills the room between the bars");
    expect(page.buttons[0].x >= page.art.right() && page.buttons[2].bottom() == page.art.bottom(),
           "the buttons stack right of the cover, ending at its foot");
    expect(page.text.bottom() <= page.buttons[0].y, "the text stops above the buttons");
    expect(page.side.x > page.buttons[0].right() && page.side.right() <= 1280.0f &&
               page.side.bottom() == page.art.bottom(),
           "the column beside the buttons runs to the cover's foot");
    for (std::size_t i = 0; i < page.buttons.size(); ++i) {
        expect(need(page.buttonAt(page.buttons[i].centreX(), page.buttons[i].centreY()), "hit") ==
                   i,
               "a button's centre hits it");
    }
    expect(!page.buttonAt(page.art.centreX(), page.art.centreY()), "the cover is no button");
    const DetailsLayout crowded =
        layoutDetails(1280.0f, 800.0f, 1.5f, 90.0f, 70.0f, DetailsParts{140.0f, 10});
    expect(crowded.buttons.front().y >= 90.0f + 140.0f - 0.01f &&
               crowded.buttons.back().bottom() <= crowded.art.bottom() + 0.01f &&
               crowded.buttons[1].y > crowded.buttons[0].bottom(),
           "buttons too many for the room shrink below the title and badge, still apart");
}

void uninstallTakesASecondPress() {
    expect(std::ranges::none_of(detailsFor(kirby(true), false, now).buttons,
                                [](const DetailsButton& button) {
                                    return button.action == DetailsAction::Uninstall;
                                }),
           "a ROM is never uninstalled");
    DetailsPage page;
    page.open(detailsFor(hades(), false, now));
    page.move(2);
    expect(page.selected() == DetailsAction::Uninstall && !page.armed() &&
               page.shown(2) == "Uninstall" && page.rows().size() == page.view().rows.size(),
           "Uninstall reads as itself until pressed");
    page.arm("Steam uninstalls the Steam copy");
    expect(page.armed() && page.shown(2) == "Press A again to uninstall" && page.shown(0) == "Play",
           "armed, the button asks for the second press");
    expect(page.rows().front() == DetailsRow{"Uninstall", "Steam uninstalls the Steam copy"} &&
               page.rows().size() == page.view().rows.size() + 1,
           "and the page says first what it removes");
    page.refresh(detailsFor(hades(), true, now));
    expect(page.armed(), "a refresh that keeps the button focused keeps it armed");
    expect(page.move(-1) && !page.armed() && page.shown(2) == "Uninstall",
           "moving focus lets go of it");
    page.move(1);
    page.arm("x");
    expect(page.focusButton(3) && !page.armed(), "a pointer moving focus lets go too");
    page.focusButton(2);
    page.arm("x");
    Game gone = hades();
    gone.installed = false;
    page.refresh(detailsFor(gone, false, now));
    expect(!page.armed() && page.selected() != DetailsAction::Uninstall,
           "the button going away lets go of it");
    page.arm("x");
    page.close();
    page.open(detailsFor(hades(), false, now));
    expect(!page.armed(), "closing lets go");
}

void focus() {
    DetailsPage page;
    page.open(detailsFor(kirby(true), false, now));
    expect(page.isOpen() && page.selected() == DetailsAction::Launch, "opens on Play");
    expect(page.move(1) && page.selected() == DetailsAction::Emulator,
           "Down moves to the emulator");
    expect(page.move(-1) && page.selected() == DetailsAction::Launch, "Up goes back");
    page.move(10);
    expect(page.selected() == DetailsAction::Back && !page.move(1),
           "focus stops at the last button");
    page.refresh(detailsFor(kirby(true), true, now));
    expect(page.selected() == DetailsAction::Back, "a refresh keeps focus on the same action");
    page.refresh(detailsFor(hades(), false, now));
    expect(!page.isOpen(), "a refresh for another game closes the page");
}

} // namespace

int main() {
    lastPlayed();
    storeGame();
    states();
    romEmulator();
    arcadeRomset();
    uninstallTakesASecondPress();
    winePrefix();
    layout();
    focus();
    std::puts("details_page: ok");
    return 0;
}
