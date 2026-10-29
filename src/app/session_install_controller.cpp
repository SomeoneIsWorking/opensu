#include "session_install_controller.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

#include "secrets/secret.hpp"
#include "ui/entry_screen.hpp"
#include "ui/progress_screen.hpp"

namespace opensu::app {
namespace {

constexpr std::string_view installingId = "session.installing";
constexpr std::string_view passwordId = "session.password";
/// The most bytes a password holds.
constexpr std::size_t passwordMostLength = 256;
constexpr std::string_view title = "Install session mode";

std::string explanation() {
    return "Session mode starts openSU as its own login session: full screen on Gamescope, with "
           "no desktop under it. You come back with Power, then Switch to desktop.\n\n" +
           std::string{"This installs the session entry in "} +
           std::string{host::sddmSessionDirs.front()} + ", a small session selector at " +
           host::selectorPath + " and a sudo rule in " + host::sudoersRulePath +
           " that lets you run only that selector without a password.\n\n"
           "An administrator password is needed to install them.";
}

} // namespace

SessionInstallController::~SessionInstallController() {
    secrets::wipe(password_);
}

const gpu2d::ui::Screen* SessionInstallController::openScreen(std::string_view id) const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [id](const gpu2d::ui::Screen* screen) {
        return screen->id() == id && !screen->closing();
    });
    return found == open.end() ? nullptr : *found;
}

bool SessionInstallController::typing() const {
    return openScreen(passwordId) != nullptr;
}

void SessionInstallController::begin() {
    if (const std::string blocked = session_.installBlocker(); !blocked.empty()) {
        say_(blocked, true);
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(gpu2d::ui::confirm(std::string{title}, explanation(), "Install", [this] {
        openPassword();
    }));
}

void SessionInstallController::cancel() {
    if (const gpu2d::ui::Screen* card = openScreen(passwordId)) {
        // input-sound.md 3.4 Close: the player dismisses a panel.
        sounds_.play(audio::Effect::Close);
        navigator_.closeFrom(*card);
    }
    secrets::wipe(password_);
}

void SessionInstallController::typed(std::string text) {
    secrets::wipe(password_);
    password_ = std::move(text);
}

void SessionInstallController::openPassword() {
    sounds_.play(audio::Effect::Open);
    secrets::wipe(password_);
    navigator_.push(std::make_unique<ui::EntryScreen>(
        ui::Entry{.id = std::string{passwordId},
                  .title = std::string{title},
                  .note = "sudo installs the session files with it.",
                  .label = "Administrator password",
                  .placeholder = "Password",
                  .secret = true,
                  .mostBytes = passwordMostLength,
                  .text = [this]() -> const std::string& {
                      return password_;
                  },
                  .changed =
                      [this](std::string text) {
                          typed(std::move(text));
                      },
                  .submitted =
                      [this] {
                          submit();
                      },
                  .cancelled =
                      [this] {
                          cancel();
                      }}));
    keyboard_.open();
}

void SessionInstallController::submit() {
    if (password_.empty()) {
        say_("Type the administrator password first", true);
        return;
    }
    if (const gpu2d::ui::Screen* card = openScreen(passwordId)) {
        navigator_.closeFrom(*card);
    }
    std::string secret = std::exchange(password_, std::string{});
    navigator_.push(std::make_unique<ui::ProgressScreen>(
        std::string{installingId},
        [] {
            return ui::ProgressCard{.title = std::string{title},
                                    .line = "Installing session mode…",
                                    .fraction = std::nullopt,
                                    .leave = {}};
        },
        nullptr));
    job_ = std::async(std::launch::async, [this, secret = std::move(secret)]() mutable {
        const host::InstallResult result = session_.install(secret);
        secrets::wipe(secret);
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
    if (const gpu2d::ui::Screen* card = openScreen(installingId)) {
        navigator_.closeFrom(*card);
    }
    if (result.kind == Kind::Refused) {
        // A wrong password: the field comes back empty.
        openPassword();
        say_(result.message.empty() ? "That password was not accepted" : result.message, true);
        return;
    }
    if (result.kind == Kind::Installed) {
        say_("Session mode installed. Power now has Switch to session mode", false);
    } else {
        say_(result.message, true);
    }
}

} // namespace opensu::app
