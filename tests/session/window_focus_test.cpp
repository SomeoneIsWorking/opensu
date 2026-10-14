// Whose window Gamescope has focused, from the root's focus properties as Gamescope writes them.
#include "session/window_focus.hpp"

#include <cstdio>
#include <vector>

#include "ui/check.hpp"

namespace {

using opensu::session::describeFocus;
using opensu::session::FocusableWindow;
using opensu::session::FocusedWindow;
using opensu::session::FocusHolder;
using opensu::session::FocusParties;
using opensu::session::holderOf;
using opensu::session::parseFocusable;
using opensu::session::steamAppId;
using opensu::test::expect;

constexpr pid_t self = 100;
constexpr pid_t game = 200;
constexpr pid_t steam = 300;
constexpr std::uint64_t ownWindow = 0x400002;

void focusableWindowsAreTriples() {
    const std::vector<long> values{0x400002, 0x400002, self, 0x600004, 769, steam, 0x700001};
    expect(parseFocusable(values) ==
               std::vector<FocusableWindow>{{0x400002, 0x400002, self}, {0x600004, 769, steam}},
           "each (window, app id, pid) is read and a partial triple dropped");
    const std::vector<FocusableWindow> listed = parseFocusable(values);
    expect(describeFocus(0x600004, listed) ==
               FocusedWindow{.window = 0x600004, .appId = steamAppId, .pid = steam},
           "the focused window takes its app id and pid from the list");
    expect(describeFocus(0x900000, listed) == FocusedWindow{.window = 0x900000},
           "a window the list leaves out has neither");
    expect(describeFocus(0, listed) == FocusedWindow{}, "nothing focused");
}

void theHolderFollowsTheFocusedWindow() {
    const FocusParties shell{.self = self, .ownWindow = ownWindow, .game = {}};
    const FocusParties playing{.self = self, .ownWindow = ownWindow, .game = {game, game + 1}};
    expect(holderOf({}, shell) == FocusHolder::OpenSU, "nothing focused leaves openSU in front");
    expect(holderOf({.window = ownWindow}, shell) == FocusHolder::OpenSU, "openSU's own window");
    expect(holderOf({.window = 0x500001, .appId = 0x500001, .pid = self}, shell) ==
               FocusHolder::OpenSU,
           "another window of openSU's");
    expect(holderOf({.window = 0x600004, .appId = steamAppId, .pid = steam}, shell) ==
               FocusHolder::BigPicture,
           "a window carrying STEAM_BIGPICTURE is Big Picture");
    expect(holderOf({.window = 0x600004, .appId = steamAppId, .pid = steam}, playing) ==
               FocusHolder::BigPicture,
           "over a game too");
    expect(holderOf({.window = 0x600009, .appId = 0x600009, .pid = steam}, shell) ==
               FocusHolder::Other,
           "a Steam dialog is another window");
    expect(holderOf({.window = 0x700001, .appId = 0x700001, .pid = game + 1}, playing) ==
               FocusHolder::Game,
           "any process of the running game holds it for the game");
    expect(holderOf({.window = 0x700001, .appId = 0x700001, .pid = game + 1}, shell) ==
               FocusHolder::Other,
           "once the game is over, its pid is no one's");
    expect(holderOf({.window = 0x700002}, playing) == FocusHolder::Game,
           "an unlisted window over a game is the game's");
    expect(holderOf({.window = 0x700002}, shell) == FocusHolder::OpenSU,
           "and over the shell, the shell's");
}

} // namespace

int main() {
    focusableWindowsAreTriples();
    theHolderFollowsTheFocusedWindow();
    std::printf("window_focus: all checks passed\n");
    return 0;
}
