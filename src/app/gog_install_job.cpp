#include "gog_install_job.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "library/install_log.hpp"

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
    return (folder_ ? *folder_ : paths_.games) / gameId;
}

std::vector<std::string> GogInstallJob::arguments(const std::string& gameId) {
    if (builds_.linuxNative) {
        platform_ = "linux";
    } else if (builds_.windows) {
        platform_ = "windows";
    } else {
        throw std::runtime_error{"GOG lists no Linux or Windows build of this game"};
    }
    // Refreshes the token when it is about to expire; NotSignedIn is a runtime_error.
    (void)auth_.accessToken();
    const std::optional<library::gog::Token> token = store_.load();
    if (!token) {
        throw std::runtime_error{"GOG is not signed in"};
    }
    handedRefreshToken_ = token->refreshToken;
    authFile_.write(*token, now());
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
    records_.add(gameId, library::gog::Installed{.path = folders.front(), .platform = platform_});
    return std::nullopt;
}

} // namespace opensu::app
