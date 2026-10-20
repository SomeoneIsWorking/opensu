// Who has the pads and what Guide does, from whose window Gamescope has focused: the pad claim
// rule, the shell ignoring the pads behind another window, Big Picture closed whenever it takes the
// focus, and Guide bringing the shell back.
#include <cstdio>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "focus_follow.hpp"
#include "gamepad/pads.hpp"
#include "input/chord_set.hpp"
#include "input/shortcuts.hpp"
#include "pad_owner.hpp"
#include "ui/check.hpp"

namespace {

using opensu::app::FocusFollow;
using opensu::app::PadOwner;
using opensu::gamepad::Button;
using opensu::gamepad::Claim;
using opensu::gamepad::Event;
using opensu::session::FocusedWindow;
using opensu::session::FocusHolder;
using opensu::session::steamAppId;
using opensu::test::expect;

constexpr std::uint64_t ownWindow = 0x400002;
constexpr pid_t steam = 300;
constexpr pid_t game = 200;

void theClaimFollowsTheFocus() {
    using S = PadOwner::Situation;
    expect(PadOwner::claimFor(S{.followsFocus = true}) == Claim::Shell,
           "inside Gamescope the shell in front grabs the pads, so Steam never sees Guide");
    expect(PadOwner::claimFor(S{.followsFocus = false}) == Claim::Free,
           "outside Gamescope the shell leaves them free");
    expect(PadOwner::claimFor(S{.followsFocus = true, .guardsSteam = false}) == Claim::Free,
           "with no Steam of openSU's to guard against the shell leaves them free");
    expect(PadOwner::claimFor(S{.followsFocus = true, .holder = FocusHolder::Other}) == Claim::Free,
           "a Steam dialog in front gets the pads");
    expect(PadOwner::claimFor(S{.followsFocus = true, .holder = FocusHolder::BigPicture}) ==
               Claim::Free,
           "so does Big Picture until it is closed");
    expect(PadOwner::claimFor(S{.game = true, .followsFocus = true, .holder = FocusHolder::Game}) ==
               Claim::Game,
           "the game in front gets its virtual pads");
    expect(PadOwner::claimFor(S{.game = true, .followsFocus = false}) == Claim::Game,
           "outside Gamescope too");
    expect(PadOwner::claimFor(
               S{.game = true, .followsFocus = true, .holder = FocusHolder::Other}) == Claim::Away,
           "another window over the game gets the real pads, the virtual ones rest");
    expect(
        PadOwner::claimFor(S{
            .game = true, .followsFocus = true, .holder = FocusHolder::Other, .menuOver = true}) ==
            Claim::Game,
        "the Guide menu over it is openSU's, whatever is focused under it");

    expect(!PadOwner::listens(S{.followsFocus = true, .holder = FocusHolder::Other}),
           "the shell does not act on the pads behind another window");
    expect(PadOwner::listens(S{.followsFocus = true, .holder = FocusHolder::OpenSU}) &&
               PadOwner::listens(S{.game = true, .holder = FocusHolder::Game}),
           "it does while it or its game is in front");
}

void behindAnotherWindowOnlyGuideIsHeard() {
    const opensu::input::Shortcuts shortcuts;
    opensu::input::ChordSet chords{shortcuts};
    // The directory a hidden run reads: no pads, so nothing on the machine is touched.
    opensu::gamepad::Pads pads{PadOwner::inputDirectory(OPENSU_TEST_SCRATCH, true)};
    PadOwner owner{pads, chords};
    const auto press = [](Button button) {
        Event event;
        event.button = button;
        event.pressed = true;
        return event;
    };
    Event connected;
    connected.kind = Event::Kind::Connected;
    const std::vector<Event> incoming{press(Button::Down), press(Button::A), press(Button::Guide),
                                      connected};
    owner.follow(true, FocusHolder::OpenSU, false);
    expect(owner.admit(incoming).size() == 4, "in front, the shell takes every event");
    owner.follow(true, FocusHolder::Other, false);
    const std::vector<Event> admitted = owner.admit(incoming);
    expect(admitted.size() == 2 && admitted[0].button == Button::Guide &&
               admitted[1].kind == Event::Kind::Connected,
           "behind a Steam window: no navigation and no sounds, only Guide and controllers");
    owner.follow(true, FocusHolder::OpenSU, false);
    expect(owner.admit(incoming).size() == 4, "back in front, everything again");
}

/// Gamescope's focus as the test sets it.
class FakeFocus final : public opensu::session::FocusSource {
  public:
    explicit FakeFocus(std::optional<FocusedWindow>& next) : next_{next} {
    }
    std::optional<FocusedWindow> takeChange() override {
        return std::exchange(next_, std::nullopt);
    }

  private:
    std::optional<FocusedWindow>& next_;
};

void bigPictureIsClosedAndGuideComesBack() {
    int closes = 0;
    int returns = 0;
    std::optional<FocusedWindow> next = FocusedWindow{.window = ownWindow, .appId = {}, .pid = {}};
    {
        FocusFollow follow{FocusFollow::Hooks{.gamePids =
                                                  [] {
                                                      return std::vector<pid_t>{game};
                                                  },
                                              .closeBigPicture =
                                                  [&closes] {
                                                      ++closes;
                                                  },
                                              .takeFocus =
                                                  [&returns] {
                                                      ++returns;
                                                  }}};
        follow.service(false);
        expect(!follow.following() && follow.holder() == FocusHolder::OpenSU,
               "with nothing to follow, openSU is in front");
        follow.attach(std::make_unique<FakeFocus>(next), ownWindow);
        follow.service(false);
        expect(follow.holder() == FocusHolder::OpenSU && !follow.guideReturns(false),
               "the shell in front: Guide opens the Guide menu");

        next = FocusedWindow{.window = 0x600009, .appId = 0x600009, .pid = steam};
        follow.service(false);
        expect(follow.holder() == FocusHolder::Other && follow.guideReturns(false),
               "a Steam dialog in front: Guide brings the shell back");
        expect(!follow.guideReturns(true), "over a game, Guide opens the menu over it");
        expect(closes == 0 && returns == 0, "a Steam dialog is left alone");

        next = FocusedWindow{.window = 0x600004, .appId = steamAppId, .pid = steam};
        follow.service(false);
        follow.service(false);
        expect(follow.holder() == FocusHolder::BigPicture, "Big Picture takes the focus");
        expect(returns == 1, "the shell takes it back, once");

        next = FocusedWindow{.window = 0x700001, .appId = 0x700001, .pid = game};
        follow.service(true);
        expect(follow.holder() == FocusHolder::Game, "the launched game in front holds it");
        next = FocusedWindow{.window = 0x600004, .appId = steamAppId, .pid = steam};
        follow.service(true);
        expect(returns == 2, "Big Picture over a game is closed too");
        follow.service(false);
        expect(follow.holder() == FocusHolder::BigPicture, "a game's end alone changes nothing");
    }
    expect(closes == 2, "each time Big Picture took the focus it was closed through Steam");
}

/// The desktop's Big Picture is the player's: it is never closed, and the shell stays put.
void aDesktopBigPictureIsLeftAlone() {
    int closes = 0;
    int returns = 0;
    std::optional<FocusedWindow> next = FocusedWindow{.window = ownWindow, .appId = {}, .pid = {}};
    {
        FocusFollow follow{FocusFollow::Hooks{.gamePids =
                                                  [] {
                                                      return std::vector<pid_t>{};
                                                  },
                                              .closeBigPicture =
                                                  [&closes] {
                                                      ++closes;
                                                  },
                                              .takeFocus =
                                                  [&returns] {
                                                      ++returns;
                                                  }}};
        follow.closeBigPicture(false);
        follow.attach(std::make_unique<FakeFocus>(next), ownWindow);
        follow.service(false);
        next = FocusedWindow{.window = 0x600004, .appId = steamAppId, .pid = steam};
        follow.service(false);
        expect(follow.holder() == FocusHolder::BigPicture, "Big Picture has the focus");
    }
    expect(closes == 0 && returns == 0, "it is neither closed nor taken back");
}

} // namespace

int main() {
    aDesktopBigPictureIsLeftAlone();
    theClaimFollowsTheFocus();
    behindAnotherWindowOnlyGuideIsHeard();
    bigPictureIsClosedAndGuideComesBack();
    std::printf("focus_owner: all checks passed\n");
    return 0;
}
