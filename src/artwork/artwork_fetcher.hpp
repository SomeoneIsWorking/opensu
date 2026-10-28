// artwork_fetcher — downloads missing artwork in the background: a Steam game's library
// portrait from Steam's CDN, a GOG game's portrait cover from gamesdb (else its library tile),
// a ROM's box art from libretro-thumbnails and an Epic game's key image from its own URL. Each file
// is kept in the store and handed back for the shell to use.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include "arcade_dat.hpp"
#include "arcade_names.hpp"
#include "artwork_store.hpp"
#include "library/game.hpp"
#include "net/web_client.hpp"
#include "rom_systems.hpp"

namespace opensu::artwork {

/// Where artwork is downloaded from; tests point these at a local server.
struct RemoteSources {
    std::string libretro{"https://thumbnails.libretro.com"};
    std::string steam{"https://cdn.cloudflare.steamstatic.com/steam/apps"};
    std::string gogdb{"https://gamesdb.gog.com/platforms/gog/external_releases"};
    std::string libretroDatabase{libretroDatabaseUrl};
    std::vector<DatPin> arcadeDats{arcadeDatPins.begin(), arcadeDatPins.end()};
};

/// A game's art or the arcade names, now on disk.
struct Fetched {
    enum class Kind : std::uint8_t { Game, Names };

    Kind kind;
    /// The game's id; empty for the names, whose games are titled again by the next catalog read.
    std::string id;
    std::filesystem::path artwork;
    /// False for a game whose art will not arrive this round: its source has none (a confirmed
    /// miss), or the round ended because the network was down. `artwork` is empty then.
    bool saved{true};
};

/// Where a game's wanted artwork is in the fetcher.
enum class Stage : std::uint8_t {
    /// Not queued: on disk already, a confirmed miss, or not asked for.
    None,
    Queued,
    Downloading,
};

class ArtworkFetcher {
  public:
    /// `names` is where the arcade listing is kept; a disabled one is never fetched for.
    ArtworkFetcher(const ArtworkStore& store, const RemoteSources& sources,
                   library::roms::NameDb names = {});
    ~ArtworkFetcher();
    ArtworkFetcher(const ArtworkFetcher&) = delete;
    ArtworkFetcher& operator=(const ArtworkFetcher&) = delete;

    /// Replaces the queue with the games the store still wants. A source that cannot be reached
    /// ends the round; the next request starts another.
    void request(const std::vector<library::Game>& games);

    /// Moves the queued games with these ids to the front, in the order given.
    void prioritize(const std::vector<std::string>& gameIds);

    /// The artwork downloaded since the last call, and the games whose art settled as not coming.
    [[nodiscard]] std::vector<Fetched> take();

    /// Where a game's artwork is.
    [[nodiscard]] Stage stageOf(std::string_view gameId) const;

    /// Every game queued or downloading.
    [[nodiscard]] std::vector<std::string> pendingGames() const;

    /// Whether nothing is queued or downloading.
    [[nodiscard]] bool idle() const;

  private:
    /// How one download ended.
    enum class Outcome : std::uint8_t { Saved, Missing, Unreachable };

    /// The arcade name listings, as queued work.
    struct NamesWork {};
    using Work = std::variant<library::Game, NamesWork>;

    void run(const std::stop_token& stop);
    Outcome fetch(const library::Game& game);
    Outcome fetch(const NamesWork& names);
    /// The finished download of `work`, for the shell.
    [[nodiscard]] Fetched arrived(const Work& work) const;
    void recordMiss(const Work& work) const;
    Outcome fetchSteam(const library::Game& game, const std::string& appId);
    Outcome fetchGog(const library::Game& game);
    Outcome fetchRom(const library::Game& game);
    Outcome keep(const library::Game& game, const std::string& url);
    /// A system's libretro listing from the store or the server; null when unreachable.
    const std::vector<std::string>* index(const library::roms::RomSystem& system);

    const ArtworkStore& store_;
    RemoteSources sources_;
    net::WebClient web_;
    library::roms::NameDb names_;
    /// The worker's listings, read once per run. Worker only.
    std::map<std::string, std::vector<std::string>, std::less<>> indexes_;

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::vector<Work> queue_;
    std::vector<Fetched> done_;
    bool busy_{false};
    /// The game being downloaded, or empty.
    std::string downloading_;
    /// Last, so the worker stops before anything it reads is destroyed.
    std::jthread worker_;
};

} // namespace opensu::artwork
