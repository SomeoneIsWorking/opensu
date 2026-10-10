// session_install_controller — the Install session mode flow. A dialog explains what session mode
// is and what gets installed; Accept opens an on-screen keyboard (or the physical one) for the
// administrator password, which `host::SessionMode` hands to sudo on a worker; a wrong password
// says so and asks again; Cancel, or B with nothing typed, ends it. The password lives only in the
// keyboard's field and then in the worker's job, and is wiped as soon as sudo has had it.
#pragma once

#include <functional>
#include <future>
#include <string>
#include <string_view>

#include "audio/sound_player.hpp"
#include "gamepad/event.hpp"
#include "host/session_mode.hpp"
#include "ui/search_panel.hpp"
#include "ui/session_dialog.hpp"

namespace opensu::app {

class SessionInstallController {
  public:
    /// What the flow tells the player: `error` marks a refusal.
    using Say = std::function<void(const std::string& text, bool error)>;

    SessionInstallController(ui::SessionDialog& dialog, ui::SearchPanel& password,
                             host::SessionMode& session, audio::SoundPlayer& sounds, Say say)
        : dialog_{dialog}, password_{password}, session_{session}, sounds_{sounds},
          say_{std::move(say)} {
    }
    ~SessionInstallController();
    SessionInstallController(const SessionInstallController&) = delete;
    SessionInstallController& operator=(const SessionInstallController&) = delete;

    /// Opens the explanation, unless installing cannot start from here (then says why).
    void begin();
    /// Whether the dialog or the keyboard is up, which is when it takes every button.
    [[nodiscard]] bool active() const noexcept {
        return dialog_.isOpen() || password_.isOpen();
    }
    /// Whether the password keyboard is up, which a physical keyboard types into.
    [[nodiscard]] bool typing() const noexcept {
        return password_.isOpen();
    }

    /// A pad button while it is up.
    void act(gamepad::Button button);

    /// What a physical keyboard typed while the password keyboard is up.
    void typeText(std::string_view text);
    void backspace();
    /// Enter: submits the password.
    void confirm();
    /// Escape: ends the flow.
    void dismiss();

    /// Takes the finished install, if there is one. Main loop only.
    void service();

  private:
    void actInDialog(gamepad::Button button);
    void openPassword();
    void cancel();
    void submit();
    void finish(const host::InstallResult& result);

    ui::SessionDialog& dialog_;
    ui::SearchPanel& password_;
    host::SessionMode& session_;
    audio::SoundPlayer& sounds_;
    Say say_;
    std::future<host::InstallResult> job_;
};

} // namespace opensu::app
