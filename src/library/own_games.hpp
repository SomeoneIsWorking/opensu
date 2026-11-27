// own_games — the games the player added themselves: a Linux program, a script or an AppImage, a
// Windows `.exe` (run under Wine by `wine::prepare`, in a prefix of its own), or an entry imported
// from Steam's non-Steam shortcuts (`steam_shortcuts`) or from Lutris (`lutris_games`). openSU
// keeps the entries in `<data dir>/own-games.json` and never writes in the game's folder; removing
// an entry leaves the files alone.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"
#include "lutris_wine.hpp"

namespace opensu::library::own {

/// Where an entry came from.
enum class Origin : std::uint8_t {
    /// A file the player picked.
    File,
    /// One of Steam's non-Steam shortcuts.
    SteamShortcut,
    /// A Lutris game, started as its Lutris config says, or by Lutris for a runner openSU lacks.
    Lutris,
    /// Found on this listing in a games folder (`folder::scan`) or among the installed Flatpaks
    /// (`folder::flatpakGames`), and never kept in the records.
    Folder,
};

/// One game the player added.
struct Entry {
    /// openSU's id for it, unique in the records; the game's `sourceId`.
    std::string id;
    std::string title;
    /// The program, script, AppImage or `.exe`; for a Lutris game, what its config names on this
    /// listing, empty when Lutris starts it.
    std::filesystem::path file;
    /// What the file is started with.
    std::vector<std::string> args{};
    /// Where it starts; empty for the file's own folder.
    std::filesystem::path workingDirectory{};
    Origin origin{Origin::File};
    /// The game's id where it was imported from (the shortcut's app id, Lutris's game id); empty
    /// for a file.
    std::string originId{};
    /// The Steam app the game is (`identify`), whose cover it shows; empty when none matched.
    std::string steamAppId{};
    /// The picture its shortcut names: a file, or a theme icon's name; empty for none.
    std::filesystem::path icon{};
    /// Which icon of `icon`, as a shortcut counts them.
    std::int32_t iconIndex{0};
    /// What its shortcut runs, program first, when that is no file of `kindOf`'s: `file` is then
    /// the program it runs (`folder::programOf`), or the shortcut when that is no file.
    std::vector<std::string> command{};
    /// The file name of the game Lutris starts, which shows it is running; read from Lutris, never
    /// saved.
    std::string lutrisProgram{};
    /// The program Lutris's config names, whether openSU or Lutris starts it; read from Lutris,
    /// never saved.
    std::filesystem::path lutrisExe{};
    /// The prefix Lutris keeps the game in; read from Lutris, never saved.
    std::filesystem::path winePrefix{};
    /// "NAME=value" pairs its Lutris config sets; read from Lutris, never saved.
    std::vector<std::string> environment{};
    /// The box art its shortcut names (`boxArtKey`), `<system>/<title>` in libretro-thumbnails, as
    /// a port that is named otherwise says which game's cover it shows; empty for none.
    std::string boxArt{};
    /// The game's own folder, every file of it inside, when a games folder holds it as one: its
    /// subfolder, or its `.app` bundle; never saved.
    std::filesystem::path folder{};

    bool operator==(const Entry&) const = default;
};

/// How a file runs.
enum class Kind : std::uint8_t {
    /// An executable Linux program.
    Native,
    /// A shell script, run by itself when executable, else by the interpreter its `#!` names.
    Script,
    AppImage,
    /// A Windows program, run under Wine.
    Windows,
};

/// What `file` is, by its extension and execute bit; nothing for a file openSU cannot start.
[[nodiscard]] std::optional<Kind> kindOf(const std::filesystem::path& file);

/// Why `file` cannot be added as a game, in words for the player; empty when it can.
[[nodiscard]] std::string refusal(const std::filesystem::path& file);

/// The entry a file the player picked stands for: a shortcut's (`folder::shortcutEntry`) or the
/// file's own, untitled unless its shortcut names it; nothing, with why in `refused`, when it
/// cannot be added.
[[nodiscard]] std::optional<Entry> pickedEntry(const std::filesystem::path& file,
                                               std::string& refused);

/// `<data dir>/own-games.json`.
[[nodiscard]] std::filesystem::path recordsFile(const std::filesystem::path& dataDir);

/// The entries on disk. Every call reads or rewrites the whole file; a write is atomic.
class Records {
  public:
    explicit Records(std::filesystem::path file);

    /// Every entry, in the order added. Throws when the file is there but unreadable.
    [[nodiscard]] std::vector<Entry> all() const;
    [[nodiscard]] std::optional<Entry> find(std::string_view id) const;
    /// Adds `entry` under an id never given before and returns it, so nothing kept by a removed
    /// game's id (its cover, last played, Wine prefix) reaches a new one.
    std::string add(Entry entry) const;
    /// Removes entry `id`; false when there was none.
    bool remove(std::string_view id) const;
    /// Points entry `id` at another file; false when there is no such entry.
    bool setFile(std::string_view id, const std::filesystem::path& file) const;
    /// Gives entry `id` another title; false when there is no such entry.
    bool rename(std::string_view id, const std::string& title) const;

  private:
    struct Document {
        /// The id the next entry gets.
        long long next{1};
        std::vector<Entry> entries;
    };
    [[nodiscard]] Document read() const;
    void write(const Document& document) const;

    std::filesystem::path file_;
};

/// The game an entry stands for: `own:<id>`, always installed, its Steam app id as its
/// `artworkKey`, its launch built from the file
/// (a Windows one marked `windowsInstall`, prepared by `wine::prepare`), or, for a Lutris game with
/// no file, `lutris lutris:rungameid/<id>`, or its shortcut's `command`. A file that is gone or
/// cannot start leaves `launch` empty and says why in `unavailable`.
[[nodiscard]] Game gameOf(const Entry& entry);

/// Whether `game` is one of the player's own that runs as a program of this OS: not a Windows one
/// under Wine, here or in Lutris's prefix.
[[nodiscard]] bool nativeOwn(const Game& game) noexcept;

/// Lists the entries, what the games folders hold as games and the Flatpak games `flatpakExports`
/// export; absent while there are none. A found game whose file is an entry's already is that
/// entry. A Lutris entry takes how it starts from `lutrisGames` on each listing, read from
/// `lutris`, and, with no Steam app of its own, the app Lutris knows it as. So does a found
/// Windows game whose `.exe` a Lutris game runs, which is dropped when that game is an entry. A
/// found game one of the player's `.desktop` entries in `applications` runs (its program, or
/// another in its folder) takes that entry's program, name, icon, box art and command.
class Provider final : public library::Provider {
  public:
    Provider(Records records, lutris::Folders lutris,
             std::vector<std::filesystem::path> gameFolders,
             std::vector<std::filesystem::path> flatpakExports, std::filesystem::path applications)
        : records_{std::move(records)}, lutris_{std::move(lutris)},
          gameFolders_{std::move(gameFolders)}, flatpakExports_{std::move(flatpakExports)},
          applications_{std::move(applications)} {
    }

    [[nodiscard]] Source source() const override {
        return Source::Own;
    }
    [[nodiscard]] std::vector<Game> list(const std::stop_token& stop) override;

  private:
    /// Fills in what Lutris knows of `entries`' Lutris games and the found ones it runs.
    void takeFromLutris(std::vector<Entry>& entries) const;

    Records records_;
    lutris::Folders lutris_;
    std::vector<std::filesystem::path> gameFolders_;
    std::vector<std::filesystem::path> flatpakExports_;
    std::filesystem::path applications_;
};

} // namespace opensu::library::own
