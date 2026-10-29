#include "gog_install_job.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "library/install_log.hpp"
#include "library/store_builds.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;

std::int64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// gogdl prints this one on stdout, then exits 2 (dl/managers/v2.py).
constexpr std::string_view unableMarker = "Unable to proceed, ";

} // namespace

GogInstallJob::GogInstallJob(Options options)
    : CliInstallJob{options.gogdl, "gogdl", "gog"},
      paths_{library::gog::Paths::under(options.dataDir)},
      store_{library::gog::TokenStore::under(options.dataDir)},
      auth_{store_, std::move(options.endpoints), web_}, authFile_{paths_.gogdlAuth},
      records_{paths_.records} {
}

GogInstallJob::~GogInstallJob() {
    halt();
}

fs::path GogInstallJob::gameRoot(const std::string& gameId) const {
    return paths_.gameRoot(folder_, gameId);
}

std::vector<std::string> GogInstallJob::arguments(const std::string& gameId) {
    if (updating_) {
        platform_ = updating_->platform;
    } else if (const std::string_view platform = library::gogInstallPlatform(builds_);
               !platform.empty()) {
        platform_ = platform;
    } else {
        throw std::runtime_error{"GOG lists no " + std::string{library::gogPlatformsText} +
                                 " build of this game"};
    }
    // Refreshes the token when it is about to expire; NotSignedIn is a runtime_error.
    (void)auth_.accessToken();
    const std::optional<library::gog::Token> token = store_.load();
    if (!token) {
        throw std::runtime_error{"GOG is not signed in"};
    }
    handedRefreshToken_ = token->refreshToken;
    authFile_.write(*token, now());
    if (updating_) {
        return {"--auth-config-path",
                authFile_.file().string(),
                "update",
                gameId,
                "--platform",
                platform_,
                "--path",
                updating_->path.string()};
    }
    return {"--auth-config-path",
            authFile_.file().string(),
            "download",
            gameId,
            "--platform",
            platform_,
            "--path",
            gameRoot(gameId).string()};
}

std::optional<double> GogInstallJob::progressIn(std::string_view line) const {
    return library::install_log::progress(line);
}

std::optional<std::string> GogInstallJob::failureIn(std::string_view line) const {
    if (std::optional<std::string> logged = library::install_log::loggedError(line)) {
        return logged;
    }
    line = library::install_log::trimmed(line);
    if (line.starts_with(unableMarker)) {
        return std::string{line.substr(unableMarker.size())};
    }
    return std::nullopt;
}

std::optional<std::string> GogInstallJob::ended(const std::string& gameId, bool installed) {
    // gogdl refreshes the token itself on a download longer than its life, and GOG then rotates
    // the refresh token; keep the newer one, and leave no second copy of the token behind.
    const std::optional<library::gog::Token> held = authFile_.read();
    authFile_.remove();
    if (held && held->refreshToken != handedRefreshToken_) {
        store_.save(*held);
    }
    if (!installed) {
        return std::nullopt;
    }
    if (updating_) {
        library::gog::Installed updated = *updating_;
        updated.build = library::gog::installedBuild(updated, gameId);
        records_.add(gameId, updated);
        return std::nullopt;
    }
    // gogdl names the folder inside the one it was given; there is exactly one.
    std::vector<fs::path> folders;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{gameRoot(gameId), ec}) {
        if (entry.is_directory()) {
            folders.push_back(entry.path());
        }
    }
    if (folders.size() != 1) {
        return "gogdl left no install folder under " + gameRoot(gameId).string();
    }
    library::gog::Installed added{.path = folders.front(), .platform = platform_};
    added.build = library::gog::installedBuild(added, gameId);
    records_.add(gameId, added);
    return std::nullopt;
}

} // namespace opensu::app
