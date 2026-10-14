#include "installs.hpp"

#include <stdexcept>
#include <utility>

#include "library/epic.hpp"
#include "library/gog_installs.hpp"
#include "settings/install_folders.hpp"

namespace opensu::app {

Installs::Installs(steam::Client& steam, const std::string& legendary,
                   const GogInstallJob::Options& gog, Folders folders, Playing playing)
    : steamReady_{[&steam] {
          return steam.state() == launch::SteamState::Ready;
      }},
      steam_{steam}, steamUpdate_{steamReady_, steam.downloadControl()}, epic_{legendary},
      gog_{gog}, uninstall_{steam, legendary, gog.dataDir},
      gogRecords_{library::gog::Paths::under(gog.dataDir).records}, folders_{std::move(folders)},
      playing_{std::move(playing)} {
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
        epic_.setUpdate(false);
        job = &epic_;
    } else if (game.source == library::Source::Gog) {
        gog_.setBuilds(game.builds);
        gog_.setFolder(folder);
        gog_.setUpdate(std::nullopt);
        job = &gog_;
    }
    return job != nullptr && begin(*job, game, Work::Install);
}

bool Installs::begin(InstallJob& job, const library::Game& game, Work work) {
    if (!job.start(game.sourceId, game.title)) {
        return false;
    }
    title_ = game.title;
    last_ = &job;
    lastStore_ = game.source;
    lastWork_ = work;
    return true;
}

std::string Installs::update(const library::Game& game) {
    if (std::string refused = refusal(game, Work::Update); !refused.empty()) {
        return refused;
    }
    if (!game.updateAvailable) {
        return game.title + " is up to date";
    }
    InstallJob* job = nullptr;
    switch (game.source) {
    case library::Source::Steam:
        if (!steamReady_()) {
            return "Steam is not running";
        }
        job = &steamUpdate_;
        break;
    case library::Source::Epic:
        epic_.setUpdate(true);
        job = &epic_;
        break;
    case library::Source::Gog: {
        std::optional<library::gog::Installed> record;
        try {
            record = gogRecords_.find(game.sourceId);
        } catch (const std::runtime_error& error) {
            return error.what();
        }
        if (!record) {
            return "openSU has no record of the GOG install";
        }
        gog_.setUpdate(std::move(record));
        job = &gog_;
        break;
    }
    case library::Source::Rom:
        break;
    }
    if (job == nullptr || !begin(*job, game, Work::Update)) {
        return busy();
    }
    return {};
}

std::string Installs::busy() {
    if (!running()) {
        return {};
    }
    switch (lastWork_) {
    case Work::Install:
        break;
    case Work::Update:
        return title_ + " is still updating";
    case Work::Uninstall:
        return title_ + " is still uninstalling";
    }
    return title_ + " is still installing";
}

std::string Installs::refusal(const library::Game& game, Work work) {
    if (!supports(game.source)) {
        return work == Work::Uninstall ? "openSU never deletes ROMs"
                                       : "openSU does not update ROMs";
    }
    if (!game.installed) {
        return game.title + " is not installed";
    }
    if (playing_(game)) {
        return game.title + " is running";
    }
    return busy();
}

Removal Installs::removal(const library::Game& game) {
    if (std::string refused = refusal(game, Work::Uninstall); !refused.empty()) {
        return Removal{.refusal = std::move(refused)};
    }
    return uninstall_.removal(game, folders_(game.source));
}

std::string Installs::uninstall(const library::Game& game) {
    if (std::string refused = removal(game).refusal; !refused.empty()) {
        return refused;
    }
    uninstall_.setStore(game.source, folders_(game.source));
    if (!begin(uninstall_, game, Work::Uninstall)) {
        return busy();
    }
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
    return Status{.store = lastStore_, .work = lastWork_, .job = std::move(*job)};
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
    if (steamUpdate_.running()) {
        return &steamUpdate_;
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
