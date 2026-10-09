// The details page: what it says about a game, its buttons, its geometry and its focus.
#include "details_page.hpp"

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
    game.emulatorOptions = {EmulatorOption{"Eden", edenInstalled, LaunchSpec{"eden", {}}},
                            EmulatorOption{"Ryujinx", false, {}}};
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
    expect(view.buttons.size() == 3 && view.buttons[0].label == "Play" &&
               view.buttons[1].label == "Hide game" &&
               view.buttons[2].action == DetailsAction::Back,
           "Play, Hide, Back");
}

void states() {
    Game missing = hades();
    missing.installed = false;
    const DetailsView view = detailsFor(missing, true, now);
    expect(view.buttons[0].label == "Install", "a game that is not installed offers Install");
    expect(valueOf(view, "Status") == "Not installed, hidden", "hidden shows");
    expect(view.buttons[1].label == "Show game", "a hidden game offers to be shown");
}

void romEmulator() {
    const DetailsView view = detailsFor(kirby(true), false, now);
    expect(view.badge == "Nintendo Switch" || view.badge == "Switch",
           "a ROM's badge is its system");
    expect(valueOf(view, "Emulator") == "Eden (installed)",
           "the emulator row names it and says it is installed");
    expect(view.buttons[1].action == DetailsAction::Emulator && view.buttons[1].value == "Eden",
           "a ROM with two emulators has a button to change it");
    const DetailsView missing = detailsFor(kirby(false), false, now);
    expect(valueOf(missing, "Emulator") == "Eden (not installed)",
           "an emulator that is not installed says so");
    Game single = kirby(true);
    single.emulatorOptions.pop_back();
    expect(detailsFor(single, false, now).buttons.size() == 3,
           "one emulator leaves nothing to choose");
}

void layout() {
    const DetailsLayout page = layoutDetails(1280.0f, 800.0f, 1.5f, 90.0f, 70.0f, 3);
    expect(page.buttons.size() == 3 && page.art.y == 90.0f && page.art.bottom() == 730.0f,
           "the cover fills the room between the bars");
    expect(page.buttons[0].x >= page.art.right() && page.buttons[2].bottom() == page.art.bottom(),
           "the buttons stack right of the cover, ending at its foot");
    expect(page.text.bottom() <= page.buttons[0].y, "the text stops above the buttons");
    for (std::size_t i = 0; i < page.buttons.size(); ++i) {
        expect(need(page.buttonAt(page.buttons[i].centreX(), page.buttons[i].centreY()), "hit") ==
                   i,
               "a button's centre hits it");
    }
    expect(!page.buttonAt(page.art.centreX(), page.art.centreY()), "the cover is no button");
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
    layout();
    focus();
    std::puts("details_page: ok");
    return 0;
}
