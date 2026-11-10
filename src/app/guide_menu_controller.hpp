// guide_menu_controller — what the Guide menu holds and does: one menu over the home screen or a
// running game. Its places (the running game's actions, Keyboard among them where it can type
// into the game, then Home, Library, Devices, Downloads, Settings and Power) stand beside the
// changes made in place (volume, mute, brightness and the interface size, the profile played as),
// each of which is set here and nowhere else, and, over Home or Library, what shapes that page's
// shelf (`ShelfOptions`) and Search. Volume goes through `VolumeControl`, brightness through
// `BrightnessControl`, the interface size through `Preferences`, a game's state commands through
// `RetroSession`, power through `host::Power` and the session switch through `host::SessionMode`.
// Every step that throws something away (Close game, Load state, Reset game, Restart, Shut down,
// the session switch) asks first, one rule. Save and close keeps the game where it is for its next
// start, and the menu says when a game resumed.
// Over a game here, Invite to play and Players hold who may join and who has, with Kick. Devices,
// Downloads and Settings open over the menu, so Back from them returns to it as it was left.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "brightness_control.hpp"
#include "host/power.hpp"
#include "host/session_mode.hpp"
#include "lan/peer_book.hpp"
#include "library/sections.hpp"
#include "preferences.hpp"
#include "profiles/profile_book.hpp"
#include "retro_session.hpp"
#include "shelf_options.hpp"
#include "ui/row_widgets.hpp"
#include "ui/setting_row.hpp"
#include "volume_control.hpp"

namespace opensu::app {

/// What the menu is opened over.
struct GuideContext {
    /// A game is running; its title heads the menu.
    bool inGame{false};
    /// The game runs on another openSU and streams here: Close game ends remote play.
    bool remote{false};
    /// That game was running there before this one joined, and goes on when it leaves.
    bool joined{false};
    std::string title{};
    /// The running game has settings to open (video, core options or frame generation).
    bool gameSettings{false};
    /// The on-screen keyboard can type into the game: Keyboard.
    bool typesIntoGame{false};
    /// openSU is the login session: Power ends with Switch to desktop, not Quit.
    bool loginSession{false};
    /// Session mode is installed: Power offers Switch to session mode, else Install session
    /// mode. Outside the login session only.
    bool sessionInstalled{false};
    /// The game here can take a guest: Invite to play, listing `invitable`.
    bool takesGuests{false};
    std::vector<lan::Peer> invitable{};
    /// Who plays the game here: Players.
    std::vector<lan::Peer> guests{};
    /// The game was started for the one guest there is, and closes as it goes.
    bool closesWithGuest{false};
    /// The page the menu opens straight over, when that is Home or Library.
    std::optional<library::Section> page{};
};

class GuideMenuController final : public ui::RowActions {
  public:
    /// A page a place opens over the menu.
    struct Page {
        std::function<bool()> isOpen;
        std::function<void()> open;
        std::function<void()> close;
    };

    /// What an entry asks of the shell app.
    struct Hooks {
        /// What the menu opens over: a running game, or the home screen.
        std::function<GuideContext()> context;
        std::function<void(library::Section)> showSection;
        Page devices;
        Page downloads;
        Page settings;
        /// Opens the search panel.
        std::function<void()> openSearch;
        /// Ends the running game, or remote play.
        std::function<void()> closeGame;
        /// Opens the running game's settings over the menu.
        std::function<void()> openGameSettings;
        /// Opens the on-screen keyboard over the running game, typing into it.
        std::function<void()> openKeyboard;
        /// Keeps a Rotate screen press as the running game's own setting.
        std::function<void()> keepScreenTurn;
        /// Gives the screen the interface size the preferences now hold.
        std::function<void()> applyLayout;
        /// Closes openSU, back to the desktop.
        std::function<void()> quit;
        /// Opens the Install session mode dialog.
        std::function<void()> installSession;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// Plays as the profile `id` from now on (`ShellApp::switchProfile`).
        std::function<void(const std::string& id)> playAs;
        /// Invites `peer` to the game here (`RemotePlay::invite`).
        std::function<void(const lan::Peer& peer)> invite;
        /// Removes the guest `peerId` from the game here (`RemotePlay::kick`).
        std::function<void(const std::string& peerId)> kick;
    };

    struct Services {
        /// Null where the machine has no power service; Sleep, Restart and Shut down are left out.
        host::Power* power;
        /// Null where there is no session mode; its rows are left out.
        host::SessionMode* session;
        VolumeControl& volume;
        BrightnessControl& brightness;
        RetroSession& retro;
        Preferences& preferences;
        ShelfOptions& shelf;
        /// The profiles the Profile row steps through.
        const profiles::ProfileBook& profiles;
    };

    /// Where the focus lands as the menu opens.
    enum class Landing : std::uint8_t {
        /// The first place.
        Places,
        /// Volume: Guide + A.
        Volume,
    };

    GuideMenuController(gpu2d::ui::Navigator& navigator, Services services,
                        audio::SdlSoundPlayer& sounds, Hooks hooks)
        : navigator_{navigator}, power_{services.power}, session_{services.session},
          volume_{services.volume}, brightness_{services.brightness}, retro_{services.retro},
          preferences_{services.preferences}, shelf_{services.shelf}, profiles_{services.profiles},
          sounds_{sounds}, hooks_{std::move(hooks)} {
    }

    /// Opens the menu over what the hooks say it is over, or closes it when open.
    void toggle(Landing landing);
    /// Closes the menu and what it opened over itself, with iiSU's Close sound.
    void close();
    [[nodiscard]] bool isOpen() const;
    /// Open with no page over it, so it has the buttons.
    [[nodiscard]] bool inFront() const;
    /// Once a loop pass: a page over the menu having gone, its rows show what is true now. A game
    /// ending needs none of this: the shell closes the menu then (`closePages`).
    void service();

    // What the screens ask.
    /// The menu's heading: the game's title, or openSU's name.
    [[nodiscard]] std::string title() const;
    [[nodiscard]] bool inGame() const noexcept {
        return context_.inGame;
    }
    /// The running game's actions and the places, under their headings.
    [[nodiscard]] std::vector<ui::SettingsRow> places() const;
    /// The changes made in place, then what shapes the page behind.
    [[nodiscard]] std::vector<ui::SettingsRow> quick() const;
    /// Sleep, Restart, Shut down and the way out of openSU.
    [[nodiscard]] std::vector<ui::SettingsRow> power() const;
    /// The paired openSUs on the LAN that are not in the game here.
    [[nodiscard]] std::vector<ui::SettingsRow> invitable() const;
    /// The game's guests, each with Kick.
    [[nodiscard]] std::vector<ui::SettingsRow> players() const;
    void run(const std::string& id, int step) override;
    void setLevel(const std::string& id, int level) override;
    void openPage(const std::string& id) override;

  private:
    /// The menu on the stack, or null.
    [[nodiscard]] const gpu2d::ui::Screen* menu() const;
    /// Reads what the rows show that is slow to read: the game's state, the brightness.
    void read();
    /// Closes the page `id` opened over the menu.
    void closePage(std::string_view id);
    /// Closes the menu, then does `then`.
    void leave(const std::function<void()>& then);
    /// Opens `page` over the menu; a page already open under it is shown by closing the menu.
    void show(const Page& page);
    void runGame(const std::string& id);
    void runPower(const std::string& id);
    void runTogether(const std::string& id);
    /// Plays as the profile `step` places from the active one, wrapping.
    void stepProfile(int step);
    /// Tells the player why a change was refused; `what` names it, when it is not plain.
    void refuse(const std::string& reason, const std::string& what = {});

    gpu2d::ui::Navigator& navigator_;
    host::Power* power_;
    host::SessionMode* session_;
    VolumeControl& volume_;
    BrightnessControl& brightness_;
    RetroSession& retro_;
    Preferences& preferences_;
    ShelfOptions& shelf_;
    const profiles::ProfileBook& profiles_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    GuideContext context_;
    /// Closes the page opened over the menu, which the menu's close takes with it.
    std::function<void()> closeShown_;
    /// A page covered the menu at the last `service`.
    bool covered_{false};
    /// The local game was last told to wait for the menu.
    bool holding_{false};
    /// The running game's opensu-retro state, read as the menu opens and after each command.
    std::optional<RetroSession::Status> retroStatus_;
    std::optional<int> brightnessLevel_;
};

} // namespace opensu::app
