#include "shared_library.hpp"

#include <chrono>
#include <span>
#include <system_error>

#include "library/match_pick.hpp"
#include "library/titles.hpp"
#include "lucent/log.h"
#include "secrets/crypto.hpp"
#include "secrets/secret.hpp"

namespace opensu::lan {
namespace {

namespace fs = std::filesystem;

/// A token for `game`'s cover as it is on disk now; empty when it has none.
std::string coverToken(const library::Game& game) {
    std::error_code ec;
    if (game.artwork.empty() || !fs::is_regular_file(game.artwork, ec)) {
        return {};
    }
    const std::uintmax_t size = fs::file_size(game.artwork, ec);
    // libc++'s file clock counts in a 128-bit type, which std::to_string does not take.
    const auto written = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             fs::last_write_time(game.artwork, ec).time_since_epoch())
                             .count();
    if (ec) {
        return {};
    }
    const secrets::crypto::Bytes digest =
        secrets::crypto::sha256(game.id + "\n" + game.artwork.string() + "\n" +
                                std::to_string(size) + "\n" + std::to_string(written));
    return secrets::hexText(std::span{digest}.first(wire::coverTokenBytes));
}

} // namespace

namespace {

/// The emulators `game` can be launched with here, when it has a choice of them.
std::vector<std::string> optionsOf(const library::Game& game) {
    std::vector<std::string> options;
    if (game.source != library::Source::Rom) {
        return options;
    }
    for (const library::EmulatorOption& option : game.emulatorOptions) {
        if (option.installed && option.problem.empty()) {
            options.push_back(option.name);
        }
    }
    return options;
}

} // namespace

void SharedLibrary::share(const std::vector<library::Game>& games, std::string hosting) {
    std::vector<wire::ListedGame> listed;
    std::map<std::string, fs::path, std::less<>> covers;
    for (const library::Game& game : games) {
        if (!game.installed || game.source == library::Source::Remote) {
            continue;
        }
        wire::ListedGame entry{.id = game.id,
                               .title = game.title,
                               .cover = coverToken(game),
                               .match = library::matchTargetOf(game),
                               .platform = library::versionPlatform(game),
                               .source = library::key(game.source),
                               .options = optionsOf(game)};
        if (!entry.cover.empty()) {
            covers.emplace(entry.cover, game.artwork);
        }
        listed.push_back(std::move(entry));
    }
    lucent::info("lan", "sharing {} installed games, {} with covers", listed.size(), covers.size());
    const std::lock_guard lock{mutex_};
    games_ = std::move(listed);
    hosting_ = std::move(hosting);
    covers_ = std::move(covers);
}

void SharedLibrary::playing(std::string id, std::vector<std::string> profiles) {
    const std::lock_guard lock{mutex_};
    playingId_ = std::move(id);
    playingAs_ = std::move(profiles);
}

wire::Listing SharedLibrary::listing() const {
    const std::lock_guard lock{mutex_};
    wire::Listing listing{.games = games_, .hosting = hosting_};
    for (wire::ListedGame& game : listing.games) {
        if (game.id == playingId_) {
            game.playingAs = playingAs_;
        }
    }
    return listing;
}

std::optional<fs::path> SharedLibrary::cover(std::string_view token) const {
    const std::lock_guard lock{mutex_};
    const auto found = covers_.find(token);
    if (found == covers_.end()) {
        return std::nullopt;
    }
    return found->second;
}

} // namespace opensu::lan
