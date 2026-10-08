#include "installs.hpp"

#include <utility>

namespace iideck::app {

Installs::Installs(steam::Client& steam, std::string legendary)
    : steam_{steam}, epic_{std::move(legendary)} {
}

bool Installs::supports(library::Source source) noexcept {
    return source == library::Source::Steam || source == library::Source::Epic;
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
    return epic_.running() ? &epic_ : nullptr;
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

} // namespace iideck::app
