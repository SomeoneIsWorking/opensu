// own_games — the games the player added themselves: a Linux program, a script or an AppImage, a
// Windows `.exe` (run under Wine by `wine::prepare`, in a prefix of its own), or an entry imported
// from Steam's non-Steam shortcuts (`steam_shortcuts`) or from Lutris (`lutris_games`), which
// Lutris runs. openSU keeps the entries in `<data dir>/own-games.json` and never writes in the
// game's folder; removing an entry leaves the files alone.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"

namespace opensu::library::own {

/// Where an entry came from.
enum class Origin : std::uint8_t {
    /// A file the player picked.
    File,
    /// One of Steam's non-Steam shortcuts.
    SteamShortcut,
    /// A Lutris game, run through Lutris.
    Lutris,
};

/// One game the player added.
struct Entry {
    /// openSU's id for it, unique in the records; the game's `sourceId`.
    std::string id;
    std::string title;
    /// The program, script, AppImage or `.exe`; empty for a Lutris game.
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
/// (a Windows one marked `windowsInstall`, prepared by `wine::prepare`), or Lutris's
/// `lutris lutris:rungameid/<id>`. A file that is gone or cannot start leaves `launch` empty and
/// says why in `unavailable`.
[[nodiscard]] Game gameOf(const Entry& entry);

/// Lists the entries as games; absent while there are none.
class Provider final : public library::Provider {
  public:
    explicit Provider(Records records) : records_{std::move(records)} {
    }

    [[nodiscard]] Source source() const override {
        return Source::Own;
    }
    [[nodiscard]] std::vector<Game> list() override;

  private:
    Records records_;
};

} // namespace opensu::library::own
