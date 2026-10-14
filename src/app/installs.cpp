#include "installs.hpp"

#include <utility>

#include "library/epic.hpp"
#include "library/gog_installs.hpp"
#include "settings/install_folders.hpp"

namespace opensu::app {

Installs::Installs(steam::Client& steam, const std::string& legendary,
                   const GogInstallJob::Options& gog, Folders folders, Playing playing)
    : steam_{steam}, epic_{legendary}, gog_{gog}, uninstall_{steam, legendary, gog.dataDir},
      folders_{std::move(folders)}, playing_{std::move(playing)} {
}

bool Installs::supports(library::Source source) noexcept {
    return source == library::Source::Steam || source == library::Source::Epic ||
           source == library::Source::Gog;
}

settings::StoreFolders Installs::ownFolders(const config::Config& config,
                                            std::optional<std::filesystem::path> steamDefault) {
    settings::StoreFolders own{
        {library::Source::Gog, library::gog::Paths::under(config.dataDir).games},
        {library::Source::Epic,
         library::epic::legendaryInstallDir(config.legendaryConfigDir, config.home)}};
    if (steamDefault) {
        own.emplace(library::Source::Steam, std::move(*steamDefault));
    }
    return own;
}

std::string Installs::folderRefusal(library::Source store) const {
    const std::optional<std::filesystem::path> folder = folders_(store);
    return folder ? settings::refusal(*folder) : std::string{};
}

bool Installs::start(const library::Game& game) {
    if (running()) {
        return false;
    }
    if (!supports(game.source)) {
        return false;
    }
    InstallJob* job = nullptr;
    const std::optional<std::filesystem::path> folder = folders_(game.source);
    if (game.source == library::Source::Steam) {
        steam_.setFolder(folder);
        job = &steam_;
    } else if (game.source == library::Source::Epic) {
        epic_.setFolder(folder);
        job = &epic_;
    } else if (game.source == library::Source::Gog) {
        gog_.setBuilds(game.builds);
        gog_.setFolder(folder);
        job = &gog_;
    }
    if (job == nullptr || !job->start(game.sourceId, game.title)) {
        return false;
    }
    title_ = game.title;
    last_ = job;
    lastStore_ = game.source;
    return true;
}

std::string Installs::busy() {
    if (!running()) {
        return {};
    }
    return title_ + (last_ == &uninstall_ ? " is still uninstalling" : " is still installing");
}

Removal Installs::removal(const library::Game& game) {
    if (!supports(game.source)) {
        return Removal{.refusal = "openSU never deletes ROMs"};
    }
    if (!game.installed) {
        return Removal{.refusal = game.title + " is not installed"};
    }
    if (playing_(game)) {
        return Removal{.refusal = game.title + " is running"};
    }
    if (std::string refused = busy(); !refused.empty()) {
        return Removal{.refusal = std::move(refused)};
    }
    return uninstall_.removal(game, folders_(game.source));
}

std::string Installs::uninstall(const library::Game& game) {
    if (std::string refused = removal(game).refusal; !refused.empty()) {
        return refused;
    }
    uninstall_.setStore(game.source, folders_(game.source));
    if (!uninstall_.start(game.sourceId, game.title)) {
        return title_ + " is still uninstalling";
    }
    title_ = game.title;
    last_ = &uninstall_;
    lastStore_ = game.source;
    return {};
}

std::optional<Installs::Status> Installs::status() const {
    if (last_ == nullptr) {
        return std::nullopt;
    }
    std::optional<InstallJob::Status> job = last_->status();
    if (!job) {
        return std::nullopt;
    }
    return Status{.store = lastStore_,
                  .work = last_ == &uninstall_ ? Work::Uninstall : Work::Install,
                  .job = std::move(*job)};
}

void Installs::cancel() {
    if (InstallJob* job = current()) {
        job->cancel();
    }
}

InstallJob* Installs::current() {
    if (steam_.running()) {
        return &steam_;
    }
    if (epic_.running()) {
        return &epic_;
    }
    if (gog_.running()) {
        return &gog_;
    }
    return uninstall_.running() ? &uninstall_ : nullptr;
}

bool Installs::running() {
    return current() != nullptr;
}

std::optional<InstallJob::Report> Installs::take() {
    InstallJob* job = current();
    return job != nullptr ? job->take() : std::nullopt;
}

void Installs::decide(bool accepted) {
    if (InstallJob* job = current()) {
        job->decide(accepted);
    }
}

} // namespace opensu::app
