// The Guide menu's entries, focus, power button and list, and geometry.
#include "guide_menu.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::test::near;
using opensu::ui::GuideAction;
using opensu::ui::GuideContext;
using opensu::ui::GuideLayout;
using opensu::ui::GuideMenu;

GuideContext home() {
    return GuideContext{false, {}};
}

GuideContext inGame() {
    return GuideContext{true, "Celeste"};
}

void homeEntriesAreTheSectionsThenTheSystem() {
    GuideMenu menu;
    menu.open(home());
    std::vector<GuideAction> actions;
    for (const auto& entry : menu.entries()) {
        actions.push_back(entry.action);
    }
    expect(actions == std::vector<GuideAction>({GuideAction::Home, GuideAction::Library,
                                                GuideAction::Devices, GuideAction::Settings}),
           "Home, Library, Devices, Settings");
    expect(menu.title() == "openSU", "the heading names openSU outside a game");
    expect(menu.focus() == 0 && std::string{menu.backLabel()} == "Close",
           "it opens on the first entry and B closes it");
}

void gameEntriesResumeOrClose() {
    GuideMenu menu;
    menu.open(inGame());
    expect(menu.entries()[0].action == GuideAction::Resume &&
               menu.entries()[1].action == GuideAction::CloseGame,
           "over a game, Resume and Close game come first");
    expect(menu.entries().size() == 4 && menu.entries()[2].action == GuideAction::Devices &&
               menu.entries()[3].action == GuideAction::Settings,
           "then Devices and Settings; no sections");
    expect(menu.title() == "Celeste" && std::string{menu.backLabel()} == "Resume",
           "the game's title heads it and B resumes");
}

void movesAndStops() {
    GuideMenu menu;
    menu.open(home());
    expect(!menu.move(-1) && menu.focus() == 0, "stops at the top");
    expect(menu.move(1) && menu.focus() == 1, "moves down");
    expect(menu.move(50) && menu.focus() == menu.entries().size() - 1, "stops at the bottom");
    expect(menu.move(1) && menu.powerFocused() && !menu.move(1),
           "then the power button, and no further");
    menu.move(-1);
    expect(menu.focusEntry(2) && !menu.focusEntry(2) && !menu.focusEntry(99),
           "a pointer focuses an entry once, and not one that is not there");
}

void powerHasItsOwnList() {
    GuideMenu menu;
    menu.open(home());
    menu.move(50);
    expect(!menu.powerFocused(), "moving a lot stops on the last row");
    expect(menu.move(1) && menu.powerFocused(), "down past the last row focuses the power button");
    expect(!menu.move(1) && menu.powerFocused(), "and stops there");
    expect(menu.move(-1) && !menu.powerFocused() && menu.focus() == 3, "up returns to the list");
    menu.focusPower();
    menu.showPower();
    expect(menu.inPower() && menu.title() == "Power" && std::string{menu.backLabel()} == "Back",
           "the power list is headed Power and B goes back");
    expect(menu.entries().size() == 5 && menu.entries()[0].action == GuideAction::Sleep &&
               menu.entries()[1].action == GuideAction::Restart &&
               menu.entries()[2].action == GuideAction::ShutDown &&
               menu.entries()[3].action == GuideAction::InstallSession &&
               menu.entries()[3].label == "Install session mode" &&
               menu.entries()[4].action == GuideAction::QuitToDesktop,
           "sleep, restart, shut down, install session mode, quit to desktop");
    menu.showMain();
    expect(!menu.inPower() && menu.powerFocused(), "going back lands on the power button");
    expect(!menu.focusPower() && menu.focusEntry(1) && !menu.powerFocused() && menu.focus() == 1,
           "a pointer moves between the button and the rows");
    menu.showPower();
    expect(!menu.focusPower(), "the button is not there in the power list");
}

void sessionModeSwitchesInsteadOfQuitting() {
    GuideMenu menu;
    menu.open(GuideContext{false, {}, true});
    menu.focusPower();
    menu.showPower();
    expect(menu.entries().size() == 4 && menu.entries()[3].action == GuideAction::SwitchToDesktop &&
               menu.entries()[3].label == "Switch to desktop",
           "as the login session the list ends with Switch to desktop");
    for (const auto& entry : menu.entries()) {
        expect(entry.action != GuideAction::QuitToDesktop, "and has no Quit to desktop");
    }
}

void sessionModeIsOfferedOutsideItAsInstallOrSwitch() {
    GuideMenu menu;
    menu.open(GuideContext{false, {}, false, true});
    menu.showPower();
    expect(menu.entries().size() == 5 && menu.entries()[3].action == GuideAction::SwitchToSession &&
               menu.entries()[3].label == "Switch to session mode" &&
               menu.entries()[4].action == GuideAction::QuitToDesktop,
           "once installed the list offers Switch to session mode");
    menu.open(GuideContext{false, {}, true, true});
    menu.showPower();
    for (const auto& entry : menu.entries()) {
        expect(entry.action != GuideAction::SwitchToSession &&
                   entry.action != GuideAction::InstallSession,
               "inside session mode there is no way into it");
    }
}

void armingAsksForASecondPress() {
    GuideMenu menu;
    menu.open(home());
    menu.showPower();
    menu.move(1);
    expect(!menu.armed() && menu.shown(1) == "Restart", "nothing is armed at first");
    menu.arm();
    expect(menu.armed() && menu.shown(1) == "Restart? Press A again" && menu.shown(0) == "Sleep",
           "an armed entry asks again");
    menu.move(1);
    expect(!menu.armed(), "moving lets go of it");
    menu.arm();
    menu.focusEntry(0);
    expect(!menu.armed(), "so does a pointer");
}

void reopenStartsOver() {
    GuideMenu menu;
    menu.open(home());
    menu.showPower();
    menu.close();
    expect(!menu.isOpen(), "closes");
    menu.open(home());
    expect(!menu.inPower() && menu.focus() == 0, "a reopened menu is the main list on its start");
}

void rowsUnderThePointer() {
    const GuideLayout layout =
        opensu::ui::layoutGuide({1920.0f, 1080.0f, 2.25f}, {40.0f, 45.0f}, 3);
    near(layout.panel.width, 675.0, "the panel is 300 dp wide");
    near(layout.panel.height, 1080.0, "and as tall as the frame");
    near(layout.rows[0].y, 54.0 + 40.0 + 54.0, "the rows start below the title");
    near(layout.rows[1].y - layout.rows[0].bottom(), 13.5, "6 dp apart");
    near(layout.powerButton.x, layout.rows[0].x, "the power button lines up with the rows");
    near(layout.powerButton.width, 81.0, "36 dp square");
    near(layout.powerButton.centreY(), layout.footer.centreY(), "in the hints' row");
    near(layout.footer.bottom(), 1080.0 - layout.padding, "which ends at the bottom padding");
    expect(layout.rows.back().bottom() < layout.footer.y, "below the rows");
    for (std::size_t i = 0; i < layout.rows.size(); ++i) {
        const auto hit = layout.rowAt(layout.rows[i].centreX(), layout.rows[i].centreY());
        expect(hit && *hit == i, "a row's centre hits it");
    }
    expect(!layout.rowAt(layout.rows[0].centreX(), layout.rows[0].bottom() + 5.0f),
           "the gap between rows hits none");
    expect(!layout.rowAt(layout.panel.right() + 10.0f, layout.rows[0].centreY()),
           "outside the panel hits none");
}

void rowsShrinkToFit() {
    const GuideLayout layout = opensu::ui::layoutGuide({1280.0f, 300.0f, 1.0f}, {30.0f, 20.0f}, 8);
    expect(layout.rows.back().bottom() <= 300.0f - 20.0f, "eight rows still end above the hints");
    expect(layout.rows[0].height < 52.0f, "by giving up height together");
    expect(layout.rows.back().bottom() <= layout.footer.y, "and stop above the footer");
}

} // namespace

int main() {
    homeEntriesAreTheSectionsThenTheSystem();
    gameEntriesResumeOrClose();
    movesAndStops();
    powerHasItsOwnList();
    sessionModeSwitchesInsteadOfQuitting();
    sessionModeIsOfferedOutsideItAsInstallOrSwitch();
    armingAsksForASecondPress();
    reopenStartsOver();
    rowsUnderThePointer();
    rowsShrinkToFit();
    std::printf("guide_menu: all checks passed\n");
    return 0;
}
