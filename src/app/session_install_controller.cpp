#include "session_install_controller.hpp"

#include <chrono>
#include <utility>

#include "host/secret.hpp"
#include "keyboard_walk.hpp"

namespace opensu::app {
namespace {

ui::SessionDialogText explanation() {
    ui::SessionDialogText text;
    text.title = "Install session mode";
    text.paragraphs = {
        "Session mode starts openSU as its own login session: full screen on Gamescope, with no "
        "desktop under it. You come back with Power, then Switch to desktop.",
        std::string{"This installs the session entry in "} +
            std::string{host::sddmSessionDirs.front()} + ", a small session selector at " +
            host::selectorPath + " and a sudo rule in " + host::sudoersRulePath +
            " that lets you run only that selector without a password.",
        "An administrator password is needed to install them.",
    };
    return text;
}

} // namespace

SessionInstallController::~SessionInstallController() {
    if (job_.valid()) {
        job_.wait();
    }
}

void SessionInstallController::begin() {
    if (const std::string blocked = session_.installBlocker(); !blocked.empty()) {
        say_(blocked, true);
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    dialog_.open(explanation());
}

void SessionInstallController::cancel() {
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    password_.close();
    dialog_.close();
}

void SessionInstallController::openPassword() {
    sounds_.play(audio::Effect::Open);
    dialog_.close();
    password_.configure("Administrator password", false, true);
    password_.open({});
}

void SessionInstallController::act(gamepad::Button button) {
    if (dialog_.busy()) {
        return;
    }
    if (password_.isOpen()) {
        switch (walkKeyboard(password_, button, sounds_)) {
        case KeyboardOutcome::Confirm:
            submit();
            break;
        case KeyboardOutcome::Leave:
            cancel();
            break;
        case KeyboardOutcome::Handled:
            break;
        }
    } else if (dialog_.isOpen()) {
        actInDialog(button);
    }
}

void SessionInstallController::actInDialog(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Left:
    case gamepad::Button::Right:
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        if (dialog_.move(directionOf(button))) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::A:
        if (dialog_.focus() == ui::dialogAccept) {
            openPassword();
        } else {
            cancel();
        }
        break;
    case gamepad::Button::B:
    case gamepad::Button::Start:
        cancel();
        break;
    default:
        break;
    }
}

void SessionInstallController::typeText(std::string_view text) {
    if (password_.isOpen()) {
        static_cast<void>(password_.type(text));
    }
}

void SessionInstallController::backspace() {
    if (password_.isOpen()) {
        static_cast<void>(password_.backspace());
    }
}

void SessionInstallController::confirm() {
    if (password_.isOpen()) {
        submit();
    }
}

void SessionInstallController::dismiss() {
    if (password_.isOpen()) {
        cancel();
    }
}

void SessionInstallController::submit() {
    if (password_.text().empty()) {
        say_("Type the administrator password first", true);
        return;
    }
    std::string secret = password_.takeText();
    password_.close();
    dialog_.open(explanation());
    dialog_.showProgress("Installing session mode...");
    job_ = std::async(std::launch::async, [this, secret = std::move(secret)]() mutable {
        const host::InstallResult result = session_.install(secret);
        host::wipe(secret);
        return result;
    });
}

void SessionInstallController::service() {
    if (!job_.valid() || job_.wait_for(std::chrono::seconds{0}) != std::future_status::ready) {
        return;
    }
    finish(job_.get());
}

void SessionInstallController::finish(const host::InstallResult& result) {
    using Kind = host::InstallResult::Kind;
    if (result.kind == Kind::Refused) {
        // A wrong password: the keyboard comes back empty.
        openPassword();
        say_(result.message.empty() ? "That password was not accepted" : result.message, true);
        return;
    }
    dialog_.close();
    if (result.kind == Kind::Installed) {
        say_("Session mode installed. Power now has Switch to session mode", false);
    } else {
        say_(result.message, true);
    }
}

} // namespace opensu::app
