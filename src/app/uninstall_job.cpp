#include "uninstall_job.hpp"

#include <chrono>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>

#include "launch/command.hpp"
#include "library/install_log.hpp"
#include "lucent/log.h"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;

constexpr auto pollInterval = std::chrono::seconds{1};
/// How long Steam may take to delete the app's manifest once asked.
constexpr auto steamLimit = std::chrono::minutes{2};

/// The finished report's line, which the toast follows the title with.
constexpr std::string_view uninstalled = "uninstalled";

} // namespace

UninstallJob::UninstallJob(steam::Client& steam, std::string legendary, const fs::path& dataDir)
    : steam_{steam}, legendary_{std::move(legendary)}, paths_{library::gog::Paths::under(dataDir)},
      records_{paths_.records} {
}

UninstallJob::~UninstallJob() {
    halt();
}

Removal UninstallJob::removal(const library::Game& game,
                              const std::optional<fs::path>& folder) const {
    switch (game.source) {
    case library::Source::Steam:
        if (steam_.state() != launch::SteamState::Ready) {
            return Removal{.refusal = "Steam is not running"};
        }
        return Removal{.what = "Steam uninstalls the Steam copy"};
    case library::Source::Epic:
        return Removal{.what = "legendary deletes the Epic copy"};
    case library::Source::Gog: {
        std::optional<library::gog::Installed> record;
        try {
            record = records_.find(game.sourceId);
        } catch (const std::runtime_error& error) {
            return Removal{.refusal = error.what()};
        }
        if (!record) {
            return Removal{.refusal = "openSU has no record of the GOG install"};
        }
        if (library::gog::ownedFolder(*record, paths_.gameRoot(folder, game.sourceId))) {
            return Removal{.what = "deletes the GOG copy at " + record->path.string()};
        }
        return Removal{.what = "forgets the GOG copy; " + record->path.string() + " is kept"};
    }
    case library::Source::Rom:
        break;
    }
    return Removal{.refusal = "openSU never deletes ROMs"};
}

void UninstallJob::run(const std::stop_token& stop, const std::string& appId) {
    switch (store_) {
    case library::Source::Steam:
        uninstallSteam(stop, appId);
        return;
    case library::Source::Epic:
        uninstallEpic(stop, appId);
        return;
    case library::Source::Gog:
        uninstallGog(appId);
        return;
    case library::Source::Rom:
        break;
    }
    post(Report{.finished = true, .failure = "openSU never deletes ROMs"});
}

void UninstallJob::uninstallSteam(const std::stop_token& stop, const std::string& appId) {
    if (steam_.state() != launch::SteamState::Ready) {
        post(Report{.finished = true, .failure = "Steam is not running"});
        return;
    }
    post(Report{.line = "Steam is removing it"});
    if (std::string refused = steam_.uninstaller().uninstall(appId); !refused.empty()) {
        post(Report{.finished = true, .failure = std::move(refused)});
        return;
    }
    const auto deadline = std::chrono::steady_clock::now() + steamLimit;
    while (!stop.stop_requested()) {
        if (!steam_.hasManifest(appId)) {
            lucent::info("steam", "{} is uninstalled", title());
            post(Report{.line = std::string{uninstalled}, .finished = true});
            return;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            post(Report{.finished = true, .failure = "Steam did not remove it"});
            return;
        }
        std::this_thread::sleep_for(pollInterval);
    }
}

void UninstallJob::uninstallEpic(const std::stop_token& stop, const std::string& appName) {
    post(Report{.line = "legendary is removing it"});
    std::optional<std::string> failure;
    // -y answers legendary's confirm, as its stdin is closed.
    const std::optional<int> status = launch::runStreaming(
        legendary_, {"uninstall", appName, "-y"},
        [&failure](std::string_view line) {
            if (std::optional<std::string> reason = library::install_log::loggedError(line);
                reason && !failure) {
                failure = std::move(reason);
            }
        },
        stop);
    if (stop.stop_requested()) {
        return;
    }
    if (!status) {
        post(Report{.finished = true, .failure = "legendary is not installed"});
    } else if (*status != 0) {
        post(Report{.finished = true,
                    .failure = failure.value_or("legendary stopped with status " +
                                                std::to_string(*status))});
    } else {
        lucent::info("epic", "{} is uninstalled", title());
        post(Report{.line = std::string{uninstalled}, .finished = true});
    }
}

void UninstallJob::uninstallGog(const std::string& gameId) {
    try {
        const std::optional<library::gog::Installed> record = records_.find(gameId);
        if (!record) {
            post(Report{.finished = true, .failure = "openSU has no record of the GOG install"});
            return;
        }
        post(Report{.line = "Removing its files"});
        const fs::path root = paths_.gameRoot(folder_, gameId);
        std::string line{uninstalled};
        if (const std::optional<fs::path> owned = library::gog::ownedFolder(*record, root)) {
            std::error_code ec;
            fs::remove_all(*owned, ec);
            if (ec) {
                post(Report{.finished = true,
                            .failure = "cannot delete " + owned->string() + ": " + ec.message()});
                return;
            }
            // The game's root, once gogdl's folder was all it held.
            fs::remove(owned->parent_path(), ec);
        } else {
            line = "forgotten; " + record->path.string() + " is kept";
        }
        records_.remove(gameId);
        lucent::info("gog", "{} is {}", title(), line);
        post(Report{.line = std::move(line), .finished = true});
    } catch (const std::runtime_error& error) {
        post(Report{.finished = true, .failure = error.what()});
    }
}

} // namespace opensu::app
