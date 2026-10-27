// guide_menu_controller — what the Guide menu holds and does: one menu over the home screen or a
// running game. Its places (the running game's actions, then Home, Library, Devices, Downloads,
// Settings and Power) stand beside the changes made in place (volume, mute, brightness and the
// interface size), each of which is set here and nowhere else. Volume goes through
// `VolumeControl`, brightness through `BrightnessControl`, the interface size through
// `Preferences`, a game's state commands through `RetroSession`, power through `host::Power` and
// the session switch through `host::SessionMode`. Every step that throws something away (Close
// game, Load state, Reset game, Restart, Shut down, the session switch) asks first, one rule.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "brightness_control.hpp"
#include "host/power.hpp"
#include "host/session_mode.hpp"
#include "library/sections.hpp"
#include "preferences.hpp"
#include "retro_session.hpp"
#include "ui/row_widgets.hpp"
#include "ui/setting_row.hpp"
#include "volume_control.hpp"

namespace opensu::app {

/// What the menu is opened over.
struct GuideContext {
    /// A game is running; its title heads the menu.
    bool inGame{false};
    std::string title;
    /// The running game has settings to open (video, core options or frame generation).
    bool gameSettings{false};
    /// openSU is the login session: Power ends with Switch to desktop, not Quit.
    bool loginSession{false};
    /// Session mode is installed: Power offers Switch to session mode, else Install session
    /// mode. Outside the login session only.
    bool sessionInstalled{false};
};

class GuideMenuController final : public ui::RowActions {
  public:
    /// What an entry asks of the shell app.
    struct Hooks {
        /// What the menu opens over: a running game, or the home screen.
        std::function<GuideContext()> context;
        std::function<void(library::Section)> showSection;
        std::function<void()> openDevices;
        std::function<void()> openDownloads;
        std::function<void()> openSettings;
        /// Ends the running game.
        std::function<void()> closeGame;
        /// Opens the running game's settings over the menu.
        std::function<void()> openGameSettings;
        /// Gives the screen the interface size the preferences now hold.
        std::function<void()> applyLayout;
        /// Closes openSU, back to the desktop.
        std::function<void()> quit;
        /// Opens the Install session mode dialog.
        std::function<void()> installSession;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    struct Services {
        host::Power& power;
        host::SessionMode& session;
        VolumeControl& volume;
        BrightnessControl& brightness;
        RetroSession& retro;
        Preferences& preferences;
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
          preferences_{services.preferences}, sounds_{sounds}, hooks_{std::move(hooks)} {
    }

    /// Opens the menu over what the hooks say it is over, or closes it when open.
    void toggle(Landing landing);
    /// Closes the menu and what it opened over itself, with iiSU's Close sound.
    void close();
    [[nodiscard]] bool isOpen() const;

    // What the screens ask.
    /// The menu's heading: the game's title, or openSU's name.
    [[nodiscard]] std::string title() const;
    [[nodiscard]] bool inGame() const noexcept {
        return context_.inGame;
    }
    /// The running game's actions and the places, under their headings.
    [[nodiscard]] std::vector<ui::SettingsRow> places() const;
    /// The changes made in place.
    [[nodiscard]] std::vector<ui::SettingsRow> quick() const;
    /// Sleep, Restart, Shut down and the way out of openSU.
    [[nodiscard]] std::vector<ui::SettingsRow> power() const;
    void run(const std::string& id, int step) override;
    void setLevel(const std::string& id, int level) override;
    void openPage(const std::string& id) override;

  private:
    /// The menu on the stack, or null.
    [[nodiscard]] const gpu2d::ui::Screen* menu() const;
    /// Reads what the rows show that is slow to read: the game's state, the brightness.
    void read();
    /// Closes the menu, then does `then`.
    void leave(const std::function<void()>& then);
    void runGame(const std::string& id);
    void runPower(const std::string& id);
    /// Tells the player why a change was refused; `what` names it, when it is not plain.
    void refuse(const std::string& reason, const std::string& what = {});

    gpu2d::ui::Navigator& navigator_;
    host::Power& power_;
    host::SessionMode& session_;
    VolumeControl& volume_;
    BrightnessControl& brightness_;
    RetroSession& retro_;
    Preferences& preferences_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    GuideContext context_;
    /// The running game's opensu-retro state, read as the menu opens and after each command.
    std::optional<RetroSession::Status> retroStatus_;
    std::optional<int> brightnessLevel_;
};

} // namespace opensu::app
