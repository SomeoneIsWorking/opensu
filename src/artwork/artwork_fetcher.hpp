// artwork_fetcher — downloads missing artwork in the background: a Steam game's library
// portrait from Steam's CDN, a ROM's box art from libretro-thumbnails. Each image is kept in
// the store and handed back for the shell to show.
#pragma once

#include <condition_variable>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "artwork_store.hpp"
#include "library/game.hpp"
#include "rom_systems.hpp"
#include "web_client.hpp"

namespace iideck::artwork {

/// Where artwork is downloaded from; tests point these at a local server.
struct RemoteSources {
    std::string libretro{"https://thumbnails.libretro.com"};
    std::string steam{"https://cdn.cloudflare.steamstatic.com/steam/apps"};
};

/// A game's artwork, now on disk.
struct Fetched {
    std::string gameId;
    std::filesystem::path artwork;
};

class ArtworkFetcher {
  public:
    ArtworkFetcher(const ArtworkStore& store, const RemoteSources& sources);
    ~ArtworkFetcher();
    ArtworkFetcher(const ArtworkFetcher&) = delete;
    ArtworkFetcher& operator=(const ArtworkFetcher&) = delete;

    /// Replaces the queue with the games the store still wants art for. A source that cannot
    /// be reached ends the round; the next request starts another.
    void request(const std::vector<library::Game>& games);

    /// The artwork downloaded since the last call.
    [[nodiscard]] std::vector<Fetched> take();

    /// Whether nothing is queued or downloading.
    [[nodiscard]] bool idle() const;

  private:
    /// How one game's download ended.
    enum class Outcome : std::uint8_t { Saved, Missing, Unreachable };

    void run(const std::stop_token& stop);
    Outcome fetch(const library::Game& game);
    Outcome fetchSteam(const library::Game& game);
    Outcome fetchRom(const library::Game& game);
    Outcome keep(const library::Game& game, const std::string& url);
    /// A system's libretro listing from the store or the server; null when unreachable.
    const std::vector<std::string>* index(const library::roms::RomSystem& system);

    const ArtworkStore& store_;
    RemoteSources sources_;
    WebClient web_;
    /// The worker's listings, read once per run. Worker only.
    std::map<std::string, std::vector<std::string>, std::less<>> indexes_;

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::vector<library::Game> queue_;
    std::vector<Fetched> done_;
    bool busy_{false};
    /// Last, so the worker stops before anything it reads is destroyed.
    std::jthread worker_;
};

} // namespace iideck::artwork
