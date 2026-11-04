#include "shared_library.hpp"

#include <span>
#include <system_error>

#include "lucent/log.h"
#include "peer_crypto.hpp"
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
    const auto written = fs::last_write_time(game.artwork, ec).time_since_epoch().count();
    if (ec) {
        return {};
    }
    const crypto::Bytes digest =
        crypto::sha256(game.id + "\n" + game.artwork.string() + "\n" + std::to_string(size) + "\n" +
                       std::to_string(written));
    return secrets::hexText(std::span{digest}.first(wire::coverTokenBytes));
}

} // namespace

void SharedLibrary::share(const std::vector<library::Game>& games) {
    std::vector<wire::ListedGame> listed;
    std::map<std::string, fs::path, std::less<>> covers;
    for (const library::Game& game : games) {
        if (!game.installed || game.source == library::Source::Remote) {
            continue;
        }
        wire::ListedGame entry{game.id, game.title, coverToken(game)};
        if (!entry.cover.empty()) {
            covers.emplace(entry.cover, game.artwork);
        }
        listed.push_back(std::move(entry));
    }
    lucent::info("lan", "sharing {} installed games, {} with covers", listed.size(), covers.size());
    const std::lock_guard lock{mutex_};
    games_ = std::move(listed);
    covers_ = std::move(covers);
}

std::vector<wire::ListedGame> SharedLibrary::listing() const {
    const std::lock_guard lock{mutex_};
    return games_;
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
