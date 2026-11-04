#include "session_install_controller.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

#include "ui/progress_screen.hpp"

namespace opensu::app {
namespace {

constexpr std::string_view installingId = "session.installing";
constexpr std::string_view title = "Install session mode";

std::string explanation() {
    return "Session mode starts openSU as its own login session: full screen on Gamescope, with "
           "no desktop under it. You come back with Power, then Switch to desktop.\n\n" +
           std::string{"This installs the session entry in "} +
           std::string{host::sddmSessionDirs.front()} + ", a small session selector at " +
           host::selectorPath + " and a sudo rule in " + host::sudoersRulePath +
           " that lets you run only that selector without a password.\n\n"
           "polkit then asks for the administrator password.";
}

} // namespace

const gpu2d::ui::Screen* SessionInstallController::openScreen(std::string_view id) const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [id](const gpu2d::ui::Screen* screen) {
        return screen->id() == id && !screen->closing();
    });
    return found == open.end() ? nullptr : *found;
}

void SessionInstallController::begin() {
    if (const std::string blocked = session_.installBlocker(); !blocked.empty()) {
        say_(blocked, true);
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(gpu2d::ui::confirm(std::string{title}, explanation(), "Install", [this] {
        start();
    }));
}

void SessionInstallController::start() {
    navigator_.push(std::make_unique<ui::ProgressScreen>(
        std::string{installingId},
        [] {
            return ui::ProgressCard{.title = std::string{title},
                                    .line = "Waiting for administrator authentication…",
                                    .fraction = std::nullopt,
                                    .leave = {}};
        },
        nullptr));
    job_ = std::async(std::launch::async, [this] {
        return session_.install();
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
    switch (result.kind) {
    case Kind::Installed:
        say_("Session mode installed. Power now has Switch to session mode", false);
        return;
    case Kind::Cancelled:
        say_("Session mode was not installed: authentication was cancelled", false);
        return;
    case Kind::Failed:
        say_(result.message, true);
        return;
    }
}

} // namespace opensu::app
