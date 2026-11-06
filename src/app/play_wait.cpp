#include "play_wait.hpp"

#include <algorithm>
#include <utility>

#include "library/romset_match.hpp"
#include "ui/game_details.hpp"

namespace opensu::app {

namespace {

bool romsetPending(const library::Game& game) {
    return std::ranges::any_of(game.emulatorOptions, [](const library::EmulatorOption& option) {
        return option.romset == library::RomsetFit::Pending;
    });
}

} // namespace

PlayWait::PlayWait(Hooks hooks) : hooks_{std::move(hooks)} {}

bool PlayWait::begin(const library::Game& game, std::vector<std::string> environment,
                     const std::string& profile) {
    if (ui::emulatorToDownload(game) == nullptr && !romsetPending(game)) {
        return false;
    }
    const std::optional<EmulatorDownloads::Finished> finished = hooks_.finished();
    held_ = Held{.gameId = game.id,
                 .environment = std::move(environment),
                 .profile = profile,
                 .seenDownload = finished ? finished->serial : 0,
                 .started = {}};
    hooks_.show(game);
    service();
    return true;
}

void PlayWait::cancel() {
    if (!held_) {
        return;
    }
    if (const std::optional<EmulatorDownloads::Progress> running = hooks_.progress();
        running && running->id == held_->started) {
        hooks_.cancelDownload(running->id);
    }
    held_.reset();
}

void PlayWait::service() {
    if (!held_) {
        return;
    }
    const std::optional<library::Game> game = hooks_.find(held_->gameId);
    if (!game) {
        held_.reset();
        return;
    }
    if (!game->launch.empty()) {
        Held held = *std::exchange(held_, std::nullopt);
        hooks_.play(*game, std::move(held.environment), held.profile);
        return;
    }
    const library::EmulatorOption* missing = ui::emulatorToDownload(*game);
    if (const std::optional<EmulatorDownloads::Progress> running = hooks_.progress()) {
        const std::optional<int> percent = running->percent();
        hooks_.update("Downloading " + (missing != nullptr ? missing->name : "an emulator") + "…",
                      percent ? std::optional{*percent / 100.0} : std::nullopt);
        return;
    }
    if (missing != nullptr) {
        const std::optional<EmulatorDownloads::Finished> finished = hooks_.finished();
        if (finished && finished->serial != held_->seenDownload && finished->id == missing->id) {
            if (finished->outcome != EmulatorDownloads::Finished::Outcome::Installed) {
                held_.reset();
                return;
            }
            // Installed: the library is reading it.
            hooks_.update("Getting " + missing->name + " ready…", std::nullopt);
            return;
        }
        if (const std::string refused = hooks_.download(missing->id); !refused.empty()) {
            held_.reset();
            hooks_.refuse(refused);
            return;
        }
        held_->started = missing->id;
        hooks_.update("Downloading " + missing->name + "…", std::nullopt);
        return;
    }
    if (romsetPending(*game)) {
        hooks_.update(std::string{library::roms::checkingRomset}, std::nullopt);
        return;
    }
    held_.reset();
    hooks_.refuse(game->unavailable.empty() ? "no way to start " + game->title : game->unavailable);
}

} // namespace opensu::app
