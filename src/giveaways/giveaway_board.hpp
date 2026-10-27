// giveaway_board — the stores' free-to-keep offers, kept fresh in the background: read from the
// cache at start (`<cache>/giveaways/offers.json`), read again from the stores on one worker
// when the last read is `refreshEvery` old (four times a day) or when asked, each offer's art
// downloaded beside it (`<cache>/giveaways/art/`). A store that fails keeps its offers from the
// last read that worked. The loop reads it with `board`; `generation` changes whenever it does.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "net/web_client.hpp"
#include "offer.hpp"

namespace opensu::giveaways {

inline constexpr std::chrono::hours refreshEvery{6};
/// How long after a read that failed everywhere the stores are asked again.
inline constexpr std::chrono::minutes retryAfter{30};

struct Board {
    std::vector<Offer> offers;
    /// When the stores were last read, Unix seconds; 0 never.
    Seconds fetched{0};
    /// Each store that failed on the last read, with why ("Epic: HTTP 403").
    std::vector<std::string> failures;
    /// Each offer's art on disk, by `key(offer)`, once it is there.
    std::map<std::string, std::filesystem::path, std::less<>> art;
    /// Whether the stores are being read now.
    bool reading{false};
};

class GiveawayBoard {
  public:
    /// No offers, nothing read.
    GiveawayBoard() = default;
    /// Kept under `<cacheDir>/giveaways`; an empty `cacheDir` is nowhere.
    explicit GiveawayBoard(const std::filesystem::path& cacheDir);
    ~GiveawayBoard();
    GiveawayBoard(const GiveawayBoard&) = delete;
    GiveawayBoard& operator=(const GiveawayBoard&) = delete;
    GiveawayBoard(GiveawayBoard&&) = delete;
    GiveawayBoard& operator=(GiveawayBoard&&) = delete;

    /// Reads the stores now, unless they are being read.
    void refresh();

    [[nodiscard]] Board board() const;
    [[nodiscard]] std::uint64_t generation() const;

  private:
    void run(const std::stop_token& stop);
    /// Reads every store and keeps what came.
    void read(const std::stop_token& stop);
    /// Downloads the art of the offers that have none on disk.
    void fetchArt(const std::stop_token& stop);
    [[nodiscard]] std::filesystem::path artFile(const Offer& offer) const;

    std::filesystem::path root_;
    net::WebClient web_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    Board board_;
    bool asked_{false};
    std::chrono::system_clock::time_point retryAt_{};
    std::uint64_t generation_{0};
    /// Last, so it stops before what it uses goes.
    std::jthread worker_;
};

} // namespace opensu::giveaways
