#include "remote_library.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::lan {
namespace {

bool sameListing(const library::Game& a, const library::Game& b) {
    return a.id == b.id && a.title == b.title && a.hostName == b.hostName &&
           a.artwork == b.artwork && a.playingAs == b.playingAs && a.hostOnline == b.hostOnline &&
           a.matchAs == b.matchAs && a.remote == b.remote && a.unavailable == b.unavailable;
}

library::Game savedGame(const nlohmann::json& entry) {
    library::Game game;
    game.id = entry.at("id").get<std::string>();
    game.source = library::Source::Remote;
    game.sourceId = entry.at("sourceId").get<std::string>();
    game.title = entry.at("title").get<std::string>();
    game.hostId = entry.at("hostId").get<std::string>();
    game.hostName = entry.at("hostName").get<std::string>();
    game.hostOnline = false;
    game.installed = true;
    game.artwork = entry.at("artwork").get<std::string>();
    game.remote = library::RemoteVersion{.platform = entry.value("platform", std::string{}),
                                         .source = entry.value("source", std::string{}),
                                         .option = entry.value("option", std::string{}),
                                         .home = std::nullopt};
    if (const auto home = entry.find("home"); home != entry.end() && home->is_number_unsigned()) {
        game.remote.home = home->get<std::size_t>();
    }
    return game;
}

class RemoteProvider final : public library::Provider {
  public:
    explicit RemoteProvider(std::shared_ptr<const RemoteLibrary> library)
        : library_{std::move(library)} {
    }

    [[nodiscard]] library::Source source() const override {
        return library::Source::Remote;
    }

    [[nodiscard]] std::vector<library::Game> list(const std::stop_token& /*stop*/) override {
        return library_->games();
    }

  private:
    std::shared_ptr<const RemoteLibrary> library_;
};

} // namespace

std::vector<library::Game> remoteVersions(const Peer& host, const wire::ListedGame& listed,
                                          const std::filesystem::path& cover,
                                          const std::string& hosting,
                                          std::optional<std::size_t> home) {
    library::Game game;
    game.id = "lan:" + host.id + ":" + listed.id;
    game.source = library::Source::Remote;
    game.sourceId = listed.id;
    game.title = listed.title;
    game.hostId = host.id;
    game.hostName = host.name;
    game.playingAs = listed.playingAs;
    game.matchAs = listed.match;
    game.remote = library::RemoteVersion{
        .platform = listed.platform, .source = listed.source, .option = {}, .home = home};
    game.installed = true;
    game.artwork = cover;
    if (!hosting.empty()) {
        game.unavailable = host.name + " cannot stream it: " + hosting;
    }
    if (listed.options.size() <= 1) {
        game.remote.option = listed.options.empty() ? std::string{} : listed.options.front();
        return {std::move(game)};
    }
    std::vector<library::Game> versions;
    for (const std::string& option : listed.options) {
        library::Game version = game;
        version.id += "#" + option;
        version.remote.option = option;
        versions.push_back(std::move(version));
    }
    return versions;
}

RemoteLibrary::RemoteLibrary(std::filesystem::path file) : file_{std::move(file)} {
    std::ifstream in{file_};
    if (!in) {
        return;
    }
    try {
        for (const nlohmann::json& entry : nlohmann::json::parse(in)) {
            games_.push_back(savedGame(entry));
        }
    } catch (const nlohmann::json::exception& failure) {
        lucent::warn("lan", "{} cannot be read: {}", file_.string(), failure.what());
        games_.clear();
    }
}

void RemoteLibrary::save() const {
    if (file_.empty()) {
        return;
    }
    nlohmann::json list = nlohmann::json::array();
    for (const library::Game& game : games_) {
        list.push_back({{"id", game.id},
                        {"sourceId", game.sourceId},
                        {"title", game.title},
                        {"hostId", game.hostId},
                        {"hostName", game.hostName},
                        {"artwork", game.artwork.string()},
                        {"platform", game.remote.platform},
                        {"source", game.remote.source},
                        {"option", game.remote.option}});
        if (game.remote.home) {
            list.back()["home"] = *game.remote.home;
        }
    }
    if (std::string error; !fileio::writeWhole(file_, list.dump() + "\n", error)) {
        lucent::warn("lan", "{} cannot be written: {}", file_.string(), error);
    }
}

std::vector<library::Game> RemoteLibrary::games() const {
    const std::lock_guard lock{mutex_};
    return games_;
}

bool RemoteLibrary::replace(std::vector<library::Game> games) {
    const std::lock_guard lock{mutex_};
    const bool same = std::ranges::equal(games, games_, sameListing);
    games_ = std::move(games);
    if (!same) {
        save();
    }
    return !same;
}

bool RemoteLibrary::update(std::vector<library::Game> online, const std::vector<Peer>& paired) {
    std::set<std::string> answered;
    for (const library::Game& game : online) {
        answered.insert(game.hostId);
    }
    std::vector<library::Game> listed = std::move(online);
    for (library::Game& game : games()) {
        if (!answered.contains(game.hostId) &&
            std::ranges::find(paired, game.hostId, &Peer::id) != paired.end()) {
            game.hostOnline = false;
            game.playingAs.clear();
            listed.push_back(std::move(game));
        }
    }
    return replace(std::move(listed));
}

std::unique_ptr<library::Provider> remoteProvider(std::shared_ptr<const RemoteLibrary> library) {
    return std::make_unique<RemoteProvider>(std::move(library));
}

} // namespace opensu::lan
