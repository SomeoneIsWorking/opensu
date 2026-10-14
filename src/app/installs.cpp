#include "installs.hpp"

#include <utility>

#include "library/epic.hpp"
#include "library/gog_installs.hpp"
#include "settings/install_folders.hpp"

namespace opensu::app {

Installs::Installs(steam::Client& steam, std::string legendary, GogInstallJob::Options gog,
                   Folders folders)
    : steam_{steam}, epic_{std::move(legendary)}, gog_{std::move(gog)},
      folders_{std::move(folders)} {
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

std::optional<Installs::Status> Installs::status() const {
    if (last_ == nullptr) {
        return std::nullopt;
    }
    std::optional<InstallJob::Status> job = last_->status();
    return job ? std::optional{Status{lastStore_, std::move(*job)}} : std::nullopt;
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
    return gog_.running() ? &gog_ : nullptr;
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
