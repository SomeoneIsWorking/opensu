#include "installs.hpp"

#include <utility>

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
    return true;
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
