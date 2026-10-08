// library — the launchable catalog: everything a controller can pick from,
// independent of which backend an entry came from.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace iideck::library {

/// The backend an entry came from.
enum class Source {
    Steam,
    Epic,
    Gog,
    Rom,
};

/// The store's display name.
[[nodiscard]] std::string_view label(Source source);

/// The command that starts a game. Every source fills this in, so launching
/// needs no knowledge of which store owns a title.
struct LaunchSpec {
    std::string program;
    std::vector<std::string> args;

    [[nodiscard]] bool empty() const noexcept {
        return program.empty();
    }
};

/// One launchable entry.
struct Game {
    /// Source-qualified, so two stores holding the same title never collide.
    std::string id;
    Source source{Source::Steam};
    /// The store's own identifier: a Steam app id, an Epic slug, a GOG app name,
    /// or the system a ROM belongs to.
    std::string sourceId;
    std::string title;

    /// Whether the title can be launched now.
    bool installed{false};

    /// Artwork on disk. The shell reads it through the artwork service rather
    /// than opening these itself.
    std::filesystem::path artwork;
    std::filesystem::path artworkWide;
    /// The name artwork is looked up by remotely: a ROM's file or folder name without its
    /// extension, dump tags and all. Empty for a source looked up by `sourceId`.
    std::string artworkKey;
    /// A store's image for the game, as `https://host/stem` with no size suffix. GOG's takes
    /// `_<size>.jpg`. Empty for a source whose art is looked up another way.
    std::string artworkUrl;

    int playtimeMinutes{0};
    std::optional<std::chrono::system_clock::time_point> lastPlayed;
    bool favourite{false};

    /// A substring of the running game's command line, used to notice that it
    /// has exited. Empty when the source cannot identify its own process.
    std::string processHint;

    LaunchSpec launch;
    /// Why `launch` is empty for a game that is there to play, in words for the player.
    std::string unavailable;
};

/// Thrown by a provider whose store is not on this machine at all, which is not a fault.
class SourceAbsent : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

/// How a store fared on the last read.
enum class Availability : std::uint8_t {
    /// Not on this machine.
    Absent,
    Ready,
    /// There but unusable until the player acts, such as signing in.
    Attention,
};

struct SourceStatus {
    Source source{Source::Steam};
    Availability availability{Availability::Ready};
    /// Why the store is absent or needs attention.
    std::string detail;
};

/// One read of every provider.
struct CatalogSnapshot {
    std::vector<Game> games;
    std::vector<SourceStatus> sources;
};

/// A backend that can list launchable games. Each store has its own type; the
/// catalog only knows this shape.
class Provider {
  public:
    virtual ~Provider() = default;
    [[nodiscard]] virtual Source source() const = 0;
    [[nodiscard]] virtual std::vector<Game> list() = 0;
};

/// Merges every configured provider into the single grid the home screen shows.
class Catalog {
  public:
    void add(std::unique_ptr<Provider> provider);

    /// Reads every provider into the merged, ordered catalog and each store's status. A provider
    /// that fails is reported in its status while the others still contribute, so one broken
    /// store cannot empty the grid.
    [[nodiscard]] CatalogSnapshot refresh();

  private:
    std::vector<std::unique_ptr<Provider>> providers_;
};

/// Sorts the catalog the way the home screen reads it: installed before
/// uninstalled, most recently played first, then by title.
void order(std::vector<Game>& games);

} // namespace iideck::library