#include "installs.hpp"

#include <utility>

namespace opensu::app {

Installs::Installs(steam::Client& steam, std::string legendary, GogInstallJob::Options gog)
    : steam_{steam}, epic_{std::move(legendary)}, gog_{std::move(gog)} {
}

bool Installs::supports(library::Source source) noexcept {
    return source == library::Source::Steam || source == library::Source::Epic ||
           source == library::Source::Gog;
}

bool Installs::start(const library::Game& game) {
    if (running()) {
        return false;
    }
    InstallJob* job = nullptr;
    if (game.source == library::Source::Steam) {
        job = &steam_;
    } else if (game.source == library::Source::Epic) {
        job = &epic_;
    } else if (game.source == library::Source::Gog) {
        gog_.setBuilds(game.builds);
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
