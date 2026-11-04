// rom_shelf — the ROMs this openSU offers paired peers to import: the games of the ROM folders it
// lists (each game's file, or every file of its game folder), grouped by console under the folder
// name they are kept in, and the BIOS files its runner has for each of those consoles. The
// listing is worked out when first asked after the catalog changed, off the shell's loop. A peer
// names a file by its kind, ROM folder and path; only a listed file is read, so a path outside the
// folders listed is refused. A file's SHA-256 is worked out on a thread of the shelf's the first
// time it is asked for, and kept while the file's size and time stay. Safe from any thread.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "library/game.hpp"
#include "wire.hpp"

namespace opensu::lan {

class RomShelf {
  public:
    /// What the shelf is made from.
    struct Contents {
        /// The ROM folders the catalog reads, in its order.
        std::vector<std::filesystem::path> roots;
        /// The catalog's games; its ROMs in those folders are offered.
        std::vector<library::Game> games;
        /// The runner's BIOS folder, and the files in it each console's runner takes, by console
        /// key, inside that folder.
        std::filesystem::path biosFolder;
        std::map<std::string, std::vector<std::string>, std::less<>> bios;
    };

    RomShelf();
    ~RomShelf();
    RomShelf(const RomShelf&) = delete;
    RomShelf& operator=(const RomShelf&) = delete;
    RomShelf(RomShelf&&) = delete;
    RomShelf& operator=(RomShelf&&) = delete;

    /// Offers `contents` from now on.
    void share(Contents contents);

    /// The consoles and their games, as `/lan/roms` lists them.
    [[nodiscard]] std::vector<wire::RomConsole> listing();
    /// The size of the listed file `ask` names and its SHA-256, empty while it is being worked
    /// out; nothing when no listed file has that name.
    [[nodiscard]] std::optional<wire::RomFacts> factsOf(const wire::RomAsk& ask);
    /// At most `wire::romChunkBytes` of the listed file `ask` names, from its offset. Nothing, with
    /// `error`, when no listed file has that name, the offset is past its end, or it cannot be
    /// read.
    [[nodiscard]] std::optional<std::string> chunkOf(const wire::RomAsk& ask, std::string& error);

  private:
    struct Listed {
        std::filesystem::path file;
        std::uint64_t bytes{0};
    };
    /// A file's hash, kept for the size and time it had.
    struct Hashed {
        std::uint64_t bytes{0};
        std::int64_t written{0};
        std::string sha256;
    };

    /// The listing, worked out from `contents_` when it is not yet; with `mutex_` held.
    void build();
    [[nodiscard]] std::optional<Listed> find(const wire::RomAsk& ask);
    void hashAll(const std::stop_token& stop);

    std::mutex mutex_;
    Contents contents_;
    bool built_{false};
    std::vector<wire::RomConsole> consoles_;
    /// Each listed file by "kind root path".
    std::map<std::string, Listed, std::less<>> files_;
    std::map<std::filesystem::path, Hashed> hashes_;
    std::vector<std::filesystem::path> toHash_;
    std::condition_variable_any wake_;
    /// Last, so it stops before what it reads goes.
    std::jthread hasher_;
};

} // namespace opensu::lan
