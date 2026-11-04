#include "remote_library.hpp"

#include <algorithm>
#include <utility>

namespace opensu::lan {
namespace {

bool sameListing(const library::Game& a, const library::Game& b) {
    return a.id == b.id && a.title == b.title && a.hostName == b.hostName && a.artwork == b.artwork;
}

class RemoteProvider final : public library::Provider {
  public:
    explicit RemoteProvider(std::shared_ptr<const RemoteLibrary> library)
        : library_{std::move(library)} {
    }

    [[nodiscard]] library::Source source() const override {
        return library::Source::Remote;
    }

    [[nodiscard]] std::vector<library::Game> list() override {
        return library_->games();
    }

  private:
    std::shared_ptr<const RemoteLibrary> library_;
};

} // namespace

library::Game remoteGame(const Peer& host, const wire::ListedGame& listed,
                         const std::filesystem::path& cover) {
    library::Game game;
    game.id = "lan:" + host.id + ":" + listed.id;
    game.source = library::Source::Remote;
    game.sourceId = listed.id;
    game.title = listed.title;
    game.hostId = host.id;
    game.hostName = host.name;
    game.installed = true;
    game.artwork = cover;
    game.unavailable = std::string{remotePlayMissing};
    return game;
}

std::vector<library::Game> RemoteLibrary::games() const {
    const std::lock_guard lock{mutex_};
    return games_;
}

bool RemoteLibrary::replace(std::vector<library::Game> games) {
    const std::lock_guard lock{mutex_};
    const bool same = std::ranges::equal(games, games_, sameListing);
    games_ = std::move(games);
    return !same;
}

std::unique_ptr<library::Provider> remoteProvider(std::shared_ptr<const RemoteLibrary> library) {
    return std::make_unique<RemoteProvider>(std::move(library));
}

} // namespace opensu::lan
