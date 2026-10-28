// Home at the bottom of the navigator, driven as the shell drives it: the D-pad moves the grid's
// focus, the wheel and a page arrow turn its pages, LB and RB step the dock in the chrome, A and Y
// reach the app's hooks for the focused tile, a click on a tile focuses and opens it, a right
// click asks for its menu, a click on a breadcrumb reaches the top bar's hook, and Back stays on
// Home.
#include "home_screen.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "support/fake_interface_text.hpp"
#include "ui/check.hpp"
#include "ui/home_dock.hpp"
#include "ui/home_grid.hpp"
#include "ui/hud.hpp"
#include "ui/top_bar_chrome.hpp"

namespace {

using namespace opensu;
using gpu2d::ui::Action;
using gpu2d::ui::PointerEvent;
using gpu2d::ui::PointerKind;
using opensu::test::expect;

constexpr float width = 1280.0f;
constexpr float height = 800.0f;
constexpr float dp = 1.5f;

std::vector<library::ShelfItem> shelf() {
    std::vector<library::ShelfItem> items;
    items.emplace_back(library::Console{.system = "gb", .label = "Game Boy", .games = 3});
    for (int i = 1; i < 30; ++i) {
        library::Game game;
        game.id = "rom:" + std::to_string(i);
        game.title = "Game " + std::to_string(i);
        game.installed = true;
        items.emplace_back(std::move(game));
    }
    return items;
}

/// The title of shelf entry `index`.
std::string titleAt(std::size_t index) {
    return index == 0 ? "Game Boy" : "Game " + std::to_string(index);
}

gpu2d::Point centre(const ui::Rect& rect) {
    return gpu2d::Point{rect.centreX(), rect.centreY()};
}

struct Rig {
    test::InterfaceRig ui;
    /// The grid's own face and icons, which a layout never draws with.
    ui::InterfaceText face{"."};
    ui::IconAtlas icons{"."};
    input::Prompts prompts;
    ui::HomeGrid grid{config::HomeMode::WiiSu, face, icons};
    ui::Hud hud{prompts, ui.text, icons};
    ui::TopBarChrome topBar{hud};
    ui::HomeDock dock{prompts, ui.text, icons};
    std::vector<std::string> calls;

    Rig() {
        hud.setSize({static_cast<int>(width), static_cast<int>(height), dp});
        hud.setTrail(ui::Trail{ui::Crumb{"Home", ui::CrumbKind::Section, library::Section::Home},
                               ui::Crumb{"Game Boy", ui::CrumbKind::Folder}});
        grid.setFrame(ui::HomeGrid::Frame{.width = static_cast<int>(width),
                                          .height = static_cast<int>(height),
                                          .dp = dp,
                                          .topInset = hud.topInset(),
                                          .bottomInset = hud.bottomInset()});
        grid.setShelf(shelf());
        topBar.setHooks(ui::TopBarChrome::Hooks{.crumb =
                                                    [this](std::size_t index) {
                                                        calls.push_back("crumb " +
                                                                        std::to_string(index));
                                                    },
                                                .launcher = {}});
        topBar.setLive(ui::TopBarChrome::Live{.crumbs = true, .launchers = false});
        dock.onChoose([this](library::Section section) {
            calls.push_back("section " + std::string{library::key(section)});
            dock.setSection(section, true);
        });
        ui.navigator.addChrome(topBar);
        ui.navigator.addChrome(dock);
        ui.navigator.setRoot(std::make_unique<app::HomeScreen>(
            grid, app::HomeScreen::Hooks{.inGame =
                                             [] {
                                                 return false;
                                             },
                                         .press =
                                             [this] {
                                                 calls.push_back(
                                                     (grid.focusedFolder() ? "open " : "details ") +
                                                     grid.focusedTitle());
                                             },
                                         .details =
                                             [this] {
                                                 calls.push_back("details " + grid.focusedTitle());
                                             },
                                         .refresh = {},
                                         .options =
                                             [this] {
                                                 calls.push_back("menu " + grid.focusedTitle());
                                             },
                                         .search = {},
                                         .menu = {},
                                         .back =
                                             [this] {
                                                 calls.push_back("back");
                                             }}));
        frame();
    }

    void frame() {
        dock.setFrame(ui::HomeDock::Frame{
            .width = width, .height = height, .dp = dp, .shown = true, .live = true});
        ui.viewport = gpu2d::Rect{0.0f, 0.0f, width, height};
        ui.frame(0.5);
    }

    void point(PointerKind kind, gpu2d::Point at, float wheel = 0.0f) {
        ui.navigator.pointer(PointerEvent{kind, at, wheel});
        frame();
    }

    [[nodiscard]] ui::Rect tile(std::size_t index) const {
        const ui::HomeLayout& layout = grid.layout();
        return layout.canvasRect(index, layout.pageScroll(grid.page()));
    }

    [[nodiscard]] std::string last() const {
        return calls.empty() ? std::string{} : calls.back();
    }
};

void theDpadMovesAcrossTiles() {
    Rig rig;
    expect(rig.ui.navigator.atRoot() && rig.ui.navigator.focused() == app::HomeScreen::tilesId,
           "Home is the root and its grid holds the focus");
    const ui::GridCell start = rig.grid.layout().cellOf(rig.grid.focusIndex());
    rig.ui.navigator.act(Action::Right);
    const ui::GridCell right = rig.grid.layout().cellOf(rig.grid.focusIndex());
    expect(right.left == start.left + 1 && right.top == start.top, "Right moves one column along");
    rig.ui.navigator.act(Action::Down);
    const ui::GridCell down = rig.grid.layout().cellOf(rig.grid.focusIndex());
    expect(down.top == right.top + 1 && down.left == right.left, "Down moves one row down");
    rig.ui.navigator.act(Action::Up);
    rig.ui.navigator.act(Action::Left);
    expect(rig.grid.focusIndex() == 0 && rig.ui.navigator.focused() == app::HomeScreen::tilesId,
           "and back, the focus never leaving the grid");
}

void theWheelAndAnArrowTurnPages() {
    Rig rig;
    expect(rig.grid.pageCount() > 1 && rig.grid.page() == 0, "the shelf fills more than a page");
    rig.point(PointerKind::Wheel, centre(rig.tile(0)), -1.0f);
    expect(rig.grid.page() == 1 && rig.grid.layout().pageOf(rig.grid.focusIndex()) == 1,
           "a wheel notch towards the player turns to the next page, the focus with it");
    rig.point(PointerKind::Wheel, centre(rig.tile(rig.grid.focusIndex())), 1.0f);
    expect(rig.grid.page() == 0, "and away turns back");
    const ui::PageArrows arrows = rig.grid.layout().pageArrows(rig.grid.page());
    rig.point(PointerKind::Press, centre(test::need(arrows.next, "the first page's next arrow")));
    expect(rig.grid.page() == 1 && rig.calls.empty(), "a click on it turns the page, opening none");
}

void theShouldersStepTheDock() {
    Rig rig;
    rig.ui.navigator.act(Action::NextTab);
    expect(rig.last() == "section library", "RB steps the dock to Library");
    rig.frame();
    rig.ui.navigator.act(Action::PreviousTab);
    expect(rig.last() == "section home", "LB steps back to Home");
    expect(rig.ui.navigator.focused() == app::HomeScreen::tilesId, "the grid keeps the focus");
}

void aAndYReachTheHooks() {
    Rig rig;
    rig.ui.navigator.act(Action::Accept);
    expect(rig.last() == "open Game Boy", "A on a console opens its folder");
    rig.ui.navigator.act(Action::Right);
    const std::string game = titleAt(rig.grid.focusIndex());
    rig.ui.navigator.act(Action::Accept);
    expect(rig.last() == "details " + game, "A on a game opens its details");
    rig.ui.navigator.act(Action::Tertiary);
    expect(rig.last() == "details " + game, "and so does Y");
    rig.ui.navigator.act(Action::Options);
    expect(rig.last() == "menu " + game, "Select asks for its menu");
    rig.ui.navigator.act(Action::Back);
    expect(rig.last() == "back" && rig.ui.navigator.atRoot(), "Back backs out, staying on Home");
}

void thePointerFocusesAndOpensTiles() {
    Rig rig;
    rig.point(PointerKind::Move, centre(rig.tile(2)));
    expect(rig.grid.focusIndex() == 2 && rig.calls.empty(), "moving onto a tile focuses it");
    rig.point(PointerKind::Press, centre(rig.tile(3)));
    expect(rig.grid.focusIndex() == 3 && rig.last() == "details " + titleAt(3),
           "a click on a tile focuses and opens it");
    rig.point(PointerKind::Context, centre(rig.tile(4)));
    expect(rig.grid.focusIndex() == 4 && rig.last() == "menu " + titleAt(4),
           "a right click on one asks for its menu");
}

void aClickOnACrumbReachesTheTopBar() {
    Rig rig;
    const std::vector<ui::Rect> crumbs = rig.hud.crumbCells();
    expect(crumbs.size() == 2, "the trail shows two levels");
    rig.point(PointerKind::Move, centre(crumbs[0]));
    expect(rig.topBar.view().hovered == "topbar/crumb/0" && rig.grid.focusIndex() == 0,
           "the pointer lights the level, not a tile under it");
    rig.point(PointerKind::Press, centre(crumbs[0]));
    expect(rig.last() == "crumb 0", "a click on it goes back to it");
}

} // namespace

int main() {
    theDpadMovesAcrossTiles();
    theWheelAndAnArrowTurnPages();
    theShouldersStepTheDock();
    aAndYReachTheHooks();
    thePointerFocusesAndOpensTiles();
    aClickOnACrumbReachesTheTopBar();
    std::printf("home screen flow: all checks passed\n");
    return 0;
}
