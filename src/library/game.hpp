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

namespace opensu::library {

/// The backend an entry came from.
enum class Source : std::uint8_t {
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

/// One emulator that can run a ROM's system, installed here or not.
struct EmulatorOption {
    std::string name;
    bool installed{false};
    /// What starts the game with it; empty when it is not installed.
    LaunchSpec launch;
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
    /// `_<size>.jpg`; Epic's is a complete image URL. Empty for a source whose art is looked up
    /// another way.
    std::string artworkUrl;

    /// The platforms the store lists builds for; set by GOG, which installs the one that runs here.
    struct Builds {
        bool windows{false};
        bool linuxNative{false};
    } builds;

    int playtimeMinutes{0};
    std::optional<std::chrono::system_clock::time_point> lastPlayed;
    bool favourite{false};

    /// A substring of the running game's command line, used to notice that it
    /// has exited. Empty when the source cannot identify its own process.
    std::string processHint;

    /// The stores a shelf reports the title as owned in, for the tile's store icons, in Steam,
    /// GOG, Epic order. Set by the shelf builders; empty in the catalog and for a ROM.
    std::vector<Source> ownedIn;

    LaunchSpec launch;
    /// A ROM's emulators for its system and the name of the one `launch` runs; empty for a game of
    /// a store, and `emulator` empty when none is installed.
    std::vector<EmulatorOption> emulatorOptions;
    std::string emulator;
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
    /// Present, and its first listing has not come back yet.
    Loading,
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

/// What one provider listed, or why it could not.
struct SourceListing {
    SourceStatus status;
    std::vector<Game> games;
};

/// A backend that can list launchable games. Each store has its own type; the
/// catalog only knows this shape.
class Provider {
  public:
    virtual ~Provider() = default;
    [[nodiscard]] virtual Source source() const = 0;
    [[nodiscard]] virtual std::vector<Game> list() = 0;
};

/// Reads one provider. A provider that fails is reported in the status, with no games, so one
/// broken store cannot empty the grid. Blocks as long as the provider does.
[[nodiscard]] SourceListing readProvider(Provider& provider);

/// The providers of every store the catalog reads, in the order the stores are shown.
class Catalog {
  public:
    void add(std::unique_ptr<Provider> provider);

    /// Hands the providers over, to whatever reads them.
    [[nodiscard]] std::vector<std::unique_ptr<Provider>> release() &&;

  private:
    std::vector<std::unique_ptr<Provider>> providers_;
};

/// The listings merged into the single catalog the home screen shows: games of the same id once,
/// ordered by `order`, and each store's status in the order of `sources`. A store in `sources`
/// without a listing is `Loading`.
[[nodiscard]] CatalogSnapshot assemble(const std::vector<Source>& sources,
                                       const std::vector<SourceListing>& listings);

/// Sorts the catalog the way the home screen reads it: installed before
/// uninstalled, most recently played first, then by title.
void order(std::vector<Game>& games);

} // namespace opensu::library