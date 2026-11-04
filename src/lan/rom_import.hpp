// rom_import — ROMs and BIOS files a paired host offers (`RomShelf`), taken here one game at a time
// on a thread of its own. Each file comes in chunks over `/lan/roms/chunk`, each sealed like every
// signed answer, into `<staging>/<host>/…/<path>.part`, never whole in memory; a dropped
// connection waits for the host and goes on from what the part holds, and so does an import of
// the same file asked again. A file whose SHA-256 is not the host's is thrown away; one that
// matches is handed to `place`, which puts it in this openSU's ROM or BIOS folder.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "net/web_client.hpp"
#include "peer_client.hpp"
#include "secrets/crypto.hpp"
#include "wire.hpp"

namespace opensu::lan {

class RomImport {
  public:
    /// Where a host listens now and the secret of the pairing with it.
    struct Link {
        Endpoint endpoint;
        secrets::crypto::Bytes secret;
    };
    /// One file of an item: which of the host's files, its size there, and where it goes here:
    /// `place` (`/`-separated) under `folder`.
    struct File {
        wire::RomKind kind{wire::RomKind::Rom};
        std::uint32_t root{0};
        std::string path;
        std::uint64_t bytes{0};
        std::filesystem::path folder;
        std::string place;
    };
    /// A game, or a BIOS file, to import from `host`.
    struct Item {
        std::string id;
        std::string host;
        std::string hostName;
        std::string title;
        /// The console it is for.
        std::string system;
        std::vector<File> files;
    };
    enum class State : std::uint8_t { Queued, Fetching, Waiting, Checking, Done, Failed };
    struct Progress {
        std::string id;
        std::string host;
        std::string title;
        std::string system;
        std::uint64_t done{0};
        std::uint64_t total{0};
        State state{State::Queued};
        /// Why it waits or failed; empty otherwise.
        std::string note;
    };
    struct Setup {
        std::string selfId;
        std::filesystem::path staging;
        /// The host as it can be reached now; nothing, with why, while it cannot.
        std::function<std::optional<Link>(const std::string& host, std::string& why)> link;
        /// Puts `staged`, checked, where `file` goes; false with `error` when it cannot.
        std::function<bool(const Item& item, const File& file, const std::filesystem::path& staged,
                           std::string& error)>
            place;
        /// An item's progress or state changed; on the import's thread.
        std::function<void()> changed;
    };

    RomImport(Setup setup, const net::WebClient& web);
    ~RomImport();
    RomImport(const RomImport&) = delete;
    RomImport& operator=(const RomImport&) = delete;
    RomImport(RomImport&&) = delete;
    RomImport& operator=(RomImport&&) = delete;

    /// Imports `items` after those already asked for; an item asked for again that is not under
    /// way starts over, from what its parts hold.
    void add(std::vector<Item> items);
    /// Stops importing from `host`: its items are dropped and their parts deleted.
    void cancel(const std::string& host);
    /// Every item asked for this run, in order.
    [[nodiscard]] std::vector<Progress> progress() const;

  private:
    struct Entry {
        Item item;
        Progress progress;
    };

    void run(const std::stop_token& stop);
    /// Imports `item` (a copy of the queued one); its state is set as it goes.
    void fetch(const Item& item, const std::stop_token& stop);
    /// One file into its part, checked and placed, the item's files before it `before` bytes.
    /// False with `error`; `lasting` when trying again cannot help.
    [[nodiscard]] bool fetchFile(const Item& item, const File& file, std::uint64_t before,
                                 const std::stop_token& stop, std::string& error, bool& lasting);
    [[nodiscard]] std::filesystem::path partOf(const std::string& host, const File& file) const;
    void update(const std::string& id, State state, std::string note);
    void reached(const std::string& id, std::uint64_t done);
    /// Whether the item is still wanted.
    [[nodiscard]] bool wanted(const std::string& id) const;
    /// Waits `wait`, or less when stopped or the item is dropped.
    void rest(std::chrono::milliseconds wait, const std::string& id, const std::stop_token& stop);

    Setup setup_;
    const net::WebClient& web_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::vector<Entry> entries_;
    /// Last, so it stops before what it reads goes.
    std::jthread worker_;
};

[[nodiscard]] std::string_view label(RomImport::State state) noexcept;

} // namespace opensu::lan
