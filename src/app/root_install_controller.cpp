#include "root_install_controller.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

#include "ui/progress_screen.hpp"

namespace opensu::app {

const gpu2d::ui::Screen* RootInstallController::openScreen(std::string_view id) const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [id](const gpu2d::ui::Screen* screen) {
        return screen->id() == id && !screen->closing();
    });
    return found == open.end() ? nullptr : *found;
}

void RootInstallController::begin() {
    if (const std::string blocked = install_.installBlocker(); !blocked.empty()) {
        say_(blocked, true);
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(gpu2d::ui::confirm(words_.title, words_.explanation, "Install", [this] {
        start();
    }));
}

RootInstallController::~RootInstallController() {
    if (job_.valid() && agent_ != nullptr) {
        agent_->shutDown();
    }
}

void RootInstallController::start() {
    navigator_.push(std::make_unique<ui::ProgressScreen>(
        words_.cardId,
        [this] {
            return ui::ProgressCard{.title = words_.title,
                                    .line = "Waiting for administrator authentication…",
                                    .fraction = std::nullopt,
                                    .leave = {}};
        },
        nullptr));
    job_ = std::async(std::launch::async, [this] {
        return install_.install();
    });
}

bool RootInstallController::service() {
    if (!job_.valid() || job_.wait_for(std::chrono::seconds{0}) != std::future_status::ready) {
        return false;
    }
    return finish(job_.get());
}

bool RootInstallController::finish(const host::InstallResult& result) {
    using Kind = host::InstallResult::Kind;
    if (const gpu2d::ui::Screen* card = openScreen(words_.cardId)) {
        navigator_.closeFrom(*card);
    }
    switch (result.kind) {
    case Kind::Installed:
        say_(words_.installed, false);
        return true;
    case Kind::Cancelled:
        say_(result.message + ": authentication was cancelled", false);
        return false;
    case Kind::Failed:
        say_(result.message, true);
        return false;
    }
    return false;
}

} // namespace opensu::app
