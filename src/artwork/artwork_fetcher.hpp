// artwork_fetcher — downloads missing artwork in the background: a Steam game's library
// portrait from Steam's CDN, a ROM's box art from libretro-thumbnails, an Epic game's key image
// from its own URL, a console's card from iiSU's starter pack, its frame glyph from iiSU's border
// pack, and its UI sounds and dock icons from the APK as they are. Each file is kept in the store
// and handed back for the shell to use.
#pragma once

#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "apk_archive.hpp"
#include "artwork_store.hpp"
#include "console_glyphs.hpp"
#include "iisu_assets.hpp"
#include "library/game.hpp"
#include "library/shelf.hpp"
#include "net/web_client.hpp"
#include "rom_systems.hpp"
#include "starter_pack.hpp"

namespace iideck::artwork {

/// Where artwork is downloaded from; tests point these at a local server.
struct RemoteSources {
    std::string libretro{"https://thumbnails.libretro.com"};
    std::string steam{"https://cdn.cloudflare.steamstatic.com/steam/apps"};
    std::string iisuApk{iisuApkUrl};
    PackPin iisuPin{iisuPackPin};
};

/// A game's, console's, system glyph's, UI sound's or dock icon's file, now on disk.
struct Fetched {
    enum class Kind : std::uint8_t { Game, Console, Glyph, Sound, NavIcon };

    Kind kind;
    /// The game's id, the console's or glyph's system, or the sound's or icon's file name.
    std::string id;
    std::filesystem::path artwork;
};

class ArtworkFetcher {
  public:
    ArtworkFetcher(const ArtworkStore& store, const RemoteSources& sources);
    ~ArtworkFetcher();
    ArtworkFetcher(const ArtworkFetcher&) = delete;
    ArtworkFetcher& operator=(const ArtworkFetcher&) = delete;

    /// Replaces the queue with the games, consoles, console glyphs and APK assets the store still
    /// wants. A source that cannot be reached ends the round; the next request starts another.
    void request(const std::vector<library::Game>& games,
                 const std::vector<library::Console>& consoles,
                 const std::vector<ApkAsset>& assets);

    /// The artwork downloaded since the last call.
    [[nodiscard]] std::vector<Fetched> take();

    /// Whether nothing is queued or downloading.
    [[nodiscard]] bool idle() const;

  private:
    /// How one download ended.
    enum class Outcome : std::uint8_t { Saved, Missing, Unreachable };

    /// A system's frame glyph, as queued work.
    struct GlyphWork {
        std::string system;
    };
    using Work = std::variant<library::Game, library::Console, GlyphWork, ApkAsset>;

    void run(const std::stop_token& stop);
    Outcome fetch(const library::Game& game);
    Outcome fetch(const library::Console& console);
    Outcome fetch(const GlyphWork& glyph);
    Outcome fetch(const ApkAsset& asset);
    /// How an APK file ended: `keep` stores a found file's bytes.
    Outcome kept(const ApkFile& file, std::string_view what,
                 const std::function<bool(std::string_view, std::string&)>& keep);
    /// The finished download of `work`, for the shell.
    [[nodiscard]] Fetched arrived(const Work& work) const;
    void recordMiss(const Work& work) const;
    Outcome fetchSteam(const library::Game& game);
    Outcome fetchRom(const library::Game& game);
    Outcome keep(const library::Game& game, const std::string& url);
    /// A system's libretro listing from the store or the server; null when unreachable.
    const std::vector<std::string>* index(const library::roms::RomSystem& system);

    const ArtworkStore& store_;
    RemoteSources sources_;
    net::WebClient web_;
    ApkArchive apk_;
    StarterPack pack_;
    ConsoleGlyphs glyphs_;
    /// The worker's listings, read once per run. Worker only.
    std::map<std::string, std::vector<std::string>, std::less<>> indexes_;

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::vector<Work> queue_;
    std::vector<Fetched> done_;
    bool busy_{false};
    /// Last, so the worker stops before anything it reads is destroyed.
    std::jthread worker_;
};

} // namespace iideck::artwork
