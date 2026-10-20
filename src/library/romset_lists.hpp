// romset_lists — the arcade cores' ROM-set listings fetched off the library scan. A scan asks for
// the listings it matches with (`want`) and goes on with what is kept; one thread fetches each due
// listing in turn through `RomsetDats`. Every fetch of the session stays listed with its state
// (queued, downloading with its bytes, done, failed with why and when it is tried again) for the
// Downloads page, and `ended` counts the fetches that ended so the shell reads the library again.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "emulator_catalog.hpp"
#include "net/web_client.hpp"
#include "romset_dats.hpp"

namespace opensu::library::roms {

/// How long after a failed fetch a scan asks for the listing again.
inline constexpr std::chrono::hours romsetRetryAfter{1};

/// One listing's fetch as it stands.
struct RomsetFetch {
    enum class Phase : std::uint8_t { Queued, Downloading, Done, Failed };

    /// The core's catalog id and name.
    std::string id;
    std::string name;
    Phase phase{Phase::Queued};
    /// Bytes received and to receive, all of the listing's files together; `total` 0 until known.
    std::uint64_t received{0};
    std::uint64_t total{0};
    /// Why it failed.
    std::string error;
    /// When a failed fetch is asked for again.
    RomsetDats::Clock::time_point retryAt{};

    bool operator==(const RomsetFetch&) const = default;
};

class RomsetLists {
  public:
    using Clock = RomsetDats::Clock;

    /// Listings that are nowhere: none kept, none fetched.
    RomsetLists() = default;
    /// Kept under `<cacheDir>/romsets`; an empty `cacheDir` is nowhere.
    explicit RomsetLists(const std::filesystem::path& cacheDir);
    ~RomsetLists();
    RomsetLists(const RomsetLists&) = delete;
    RomsetLists& operator=(const RomsetLists&) = delete;
    RomsetLists(RomsetLists&&) = delete;
    RomsetLists& operator=(RomsetLists&&) = delete;

    [[nodiscard]] const RomsetDats& dats() const noexcept {
        return dats_;
    }

    /// Queues a fetch of `entry`'s listing when it is due (`RomsetDats::due`) at `now`, unless it
    /// is queued, downloading, or failed less than `romsetRetryAfter` before.
    void want(const EmulatorEntry& entry, Clock::time_point now);

    /// Whether `id`'s listing is queued or downloading.
    [[nodiscard]] bool fetching(std::string_view id) const;

    /// Every fetch of this session, in the order first asked for.
    [[nodiscard]] std::vector<RomsetFetch> fetches() const;

    /// How many fetches have ended, done or failed.
    [[nodiscard]] std::uint64_t ended() const;

  private:
    void run(const std::stop_token& stop);
    /// Fetches `entry`'s listing and records how it went.
    void fetch(const EmulatorEntry& entry, const std::stop_token& stop);
    [[nodiscard]] RomsetFetch* find(std::string_view id);

    net::WebClient web_;
    RomsetDats dats_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::vector<RomsetFetch> fetches_;
    std::deque<EmulatorEntry> queue_;
    std::uint64_t ended_{0};
    /// Last, so it stops before what it uses goes.
    std::jthread worker_;
};

} // namespace opensu::library::roms
