// library — the launchable catalog: everything a controller can pick from,
// independent of which backend an entry came from.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
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
    /// A game the player added from a file, or imported from Steam's shortcuts or Lutris.
    Own,
    /// A game on another openSU on the LAN that this one paired with.
    Remote,
};

/// The store's display name.
[[nodiscard]] std::string_view label(Source source);

/// The store's name as keys spell it: "steam", "epic", "gog", "emulator", "own", "lan".
[[nodiscard]] std::string key(Source source);

/// The command that starts a game. Every source fills this in, so launching
/// needs no knowledge of which store owns a title.
/// A romset rebuilt for its core before the game starts (`romset_convert`).
struct RomConversion {
    /// The core's set it becomes.
    std::string target;
    std::string emulatorId;
    /// The zips it is read from, the game's own first.
    std::vector<std::filesystem::path> zips;
    /// Changes when any of them does.
    std::string stamp;
};

struct LaunchSpec {
    std::string program;
    std::vector<std::string> args;
    /// The folder it starts in; empty for openSU's own.
    std::filesystem::path workingDirectory{};
    /// "NAME=value" pairs set over everything else the launch gives it.
    std::vector<std::string> environment{};
    /// Set when the game's zip must be rebuilt first; its location in `args` gives way to the
    /// rebuilt one.
    std::optional<RomConversion> conversion{};

    [[nodiscard]] bool empty() const noexcept {
        return program.empty();
    }
};

/// Whether an arcade core's listing has every ROM of the game's set.
enum class RomsetFit : std::uint8_t {
    /// Not an arcade core, or its listing could not be had.
    Unchecked,
    Matches,
    /// Its listing's set can be rebuilt from the zip (`RomConversion`).
    Converts,
    Mismatch,
    /// Its listing is downloading.
    Pending,
};

/// What a Switch game's control.nacp says of its saves: their owner and each kind's sizes, which
/// an emulator's new save of it is made with.
struct SwitchSaveData {
    std::uint64_t owner{0};
    std::uint64_t accountSize{0};
    std::uint64_t accountJournal{0};
    std::uint64_t deviceSize{0};
    std::uint64_t deviceJournal{0};

    bool operator==(const SwitchSaveData&) const = default;
};

/// One emulator that can run a ROM's system, installed here or not.
struct EmulatorOption {
    std::string name;
    bool installed{false};
    /// What starts the game with it; empty when it is not installed.
    LaunchSpec launch;
    /// The catalog's id; empty for a configured override.
    std::string id;
    /// Whether openSU can download it.
    bool downloadable{false};
    /// Why an emulator that is there cannot start, with what fixes it; empty otherwise.
    std::string problem;
    RomsetFit romset{RomsetFit::Unchecked};
    /// What the set lacks for this core ("lacks 2 of 16 files: a.bin, b.bin"); empty unless
    /// `romset` is `Mismatch`.
    std::string romsetGap;
    /// What its running process's command line holds: the program or AppImage it starts from, or
    /// its Flatpak's app id; empty when it is not installed.
    std::string processHint;
    /// Whether it is installed as its Flatpak, which keeps its folders under `~/.var/app`.
    bool flatpak{false};
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
    /// A ROM's game file, the one its emulator starts, or the file of a game the player added;
    /// empty for a game of a store.
    std::filesystem::path file;
    /// The id a ROM's file gives itself (`DumpInfo::titleId`): a Switch game's title id; empty
    /// when it gives none.
    std::string titleId;
    /// A Switch game's own save sizes (`DumpInfo::saveData`); nothing for another game.
    std::optional<SwitchSaveData> switchSaveData;
    /// The openSU on the LAN a remote game is on: its peer id and the name it goes by; both empty
    /// for a game on this machine.
    std::string hostId;
    std::string hostName;
    /// A remote game running there: the profiles playing a seat in it, which one of them here can
    /// continue.
    std::vector<std::string> playingAs;
    /// A remote game: what its host says it can be matched to (`matchTargetOf`).
    std::string matchAs;

    /// Whether that openSU answers now; a game listed earlier stays, offline, until it is back.
    bool hostOnline{true};
    /// Whether the title can be launched now.
    bool installed{false};
    /// The store has a newer build than the installed one; set only for an installed store game.
    bool updateAvailable{false};
    /// The store keeps the game's saves in its cloud; set by Epic, whose metadata names a cloud
    /// save folder (`CloudSaveFolder` or `CloudSaveFolder_MAC`).
    bool cloudSaves{false};

    /// Artwork on disk. The shell reads it through the artwork service rather
    /// than opening these itself.
    std::filesystem::path artwork;
    std::filesystem::path artworkWide;
    /// The name artwork is looked up by remotely: a ROM's file or folder name without its
    /// extension, dump tags and all, or the box art title picked for it; for any other game, the
    /// Steam app id it is matched to. Empty for a game looked up by `sourceId`.
    std::string artworkKey;
    /// A store's image for the game, as `https://host/stem` with no size suffix. GOG's takes
    /// `_<size>.jpg`; Epic's is a complete image URL. Empty for a source whose art is looked up
    /// another way.
    std::string artworkUrl;

    /// The platforms the store lists builds for; set by GOG, which installs the one that runs here
    /// (`gogInstallPlatform`).
    struct Builds {
        bool windows{false};
        bool linuxNative{false};
        bool mac{false};
    } builds;
    /// The installed build is a Windows one this OS runs under Wine (`wine::prepare`); GOG, Epic
    /// and an added `.exe`. Never on Windows, which runs them itself (`windowsBuildsNative`).
    bool windowsInstall{false};
    /// The Wine prefix another launcher keeps the game in, which it stays in; empty when openSU
    /// picks one (`wine::prefixFor`).
    std::filesystem::path winePrefix;
    /// A picture of the game's own that a shortcut names: a file, or a theme icon's name; empty
    /// for none, its program's own icon then standing in.
    std::filesystem::path iconSource;
    /// Which icon of `iconSource`, as a shortcut counts them.
    std::int32_t iconIndex{0};
    /// Its own icon on disk, drawn small on a tile with no cover; empty for none.
    std::filesystem::path icon;
    /// An added game's picked box art, `<system>/<title>` in libretro-thumbnails; empty for none.
    std::string boxArt;
    /// A game a games folder holds: it comes and goes with its file, so it has no entry to rename,
    /// repoint or remove.
    bool fromGameFolder{false};
    /// A game imported from Lutris, whose config names its file: that is changed in Lutris.
    bool fromLutris{false};

    int playtimeMinutes{0};
    std::optional<std::chrono::system_clock::time_point> lastPlayed;
    bool favourite{false};
    /// A store copy of a title two stores own, so what names it says which (`markCopies`).
    bool severalStores{false};
    /// A tile of the title with an update on any copy. Set by `visibleShelf`.
    bool updateOnCopy{false};

    /// What the copies of its title keep their settings, controls and chosen cover under; empty
    /// for a game that keeps them under its id (`shareTitleData`).
    std::string sharedId;

    /// A substring of the running game's command line, used to notice that it
    /// has exited. Empty when the source cannot identify its own process.
    std::string processHint;

    /// The stores a tile of the title carries icons for, in Steam, GOG, Epic order. Set by the
    /// shelf builders; empty in the catalog, on a launcher page and for a ROM.
    std::vector<Source> ownedIn;

    LaunchSpec launch;
    /// A ROM's emulators for its system and the name of the one `launch` runs; empty for a game of
    /// a store, and `emulator` empty when none is installed.
    std::vector<EmulatorOption> emulatorOptions;
    std::string emulator;
    /// Why `launch` is empty for a game that is there to play, in words for the player.
    std::string unavailable;
    /// An arcade zip's romset: the cores it matches, that none does, or that it is being checked;
    /// empty when it was not checked.
    std::string romset;
    /// An arcade zip's set as its file is named ("avspu"); empty for every other game. A picked
    /// folder's `file` is a document URI, which names no set.
    std::string romsetName;
    /// With no core matching, the nearest one and what the set lacks for it.
    std::string romsetNearest;
};

/// Thrown by a provider whose store is not on this machine at all, which is not a fault.
class SourceAbsent : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

/// Thrown through a store's code by a request its stop ended, which is not a fault: what it was
/// for is dropped and nothing is logged. Not a runtime_error, so a failure's handler passes it on.
class RequestStopped : public std::exception {
  public:
    [[nodiscard]] const char* what() const noexcept override {
        return "stopped";
    }
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

/// Whether `source` listed its games in the last read: there and signed in.
[[nodiscard]] bool isReady(const std::vector<SourceStatus>& sources, Source source);

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
    /// The store's games; a listing that waits on the network ends once `stop` is requested.
    [[nodiscard]] virtual std::vector<Game> list(const std::stop_token& stop) = 0;
};

/// Reads one provider. A provider that fails is reported in the status, with no games, so one
/// broken store cannot empty the grid. Blocks as long as the provider does; nothing when `stop`
/// ended the listing.
[[nodiscard]] std::optional<SourceListing> readProvider(Provider& provider,
                                                        const std::stop_token& stop);

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

/// Whether `text` can be a Steam app id: digits only, so a Steam script takes it as a number.
[[nodiscard]] bool isSteamAppId(std::string_view text) noexcept;

/// What `game` keeps its settings, controls and chosen cover under.
[[nodiscard]] inline const std::string& dataId(const Game& game) {
    return game.sharedId.empty() ? game.id : game.sharedId;
}

} // namespace opensu::library