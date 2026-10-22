// cover_offers — what "Choose artwork" offers for a game, on a worker: SteamGridDB's covers of
// every shape, each with its thumbnail downloaded under the cache as it arrives, and the player's
// pick downloaded whole and kept as the game's chosen cover (`CoverOverrides`).
#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "cover_overrides.hpp"
#include "library/game.hpp"
#include "net/web_client.hpp"
#include "steamgriddb.hpp"

namespace opensu::artwork {

/// One cover offered, with its thumbnail once it is on disk.
struct CoverOffer {
    SteamGridDbCover cover;
    std::filesystem::path thumb;
};

/// What is offered for one game so far.
struct CoverListing {
    std::string gameId;
    SteamGridDb::Covers::Status status{SteamGridDb::Covers::Status::Listed};
    std::vector<CoverOffer> offers;
    std::string error;
    /// Every thumbnail has been asked for.
    bool complete{false};
};

/// How a choice ended.
struct CoverChoice {
    std::string gameId;
    bool kept{false};
    std::string error;
};

class CoverOffers {
  public:
    /// The most covers offered for a game.
    static constexpr std::size_t mostOffers = 24;

    CoverOffers(const CoverOverrides& overrides, const SteamGridDb& steamGridDb,
                std::filesystem::path thumbDir);
    ~CoverOffers();
    CoverOffers(const CoverOffers&) = delete;
    CoverOffers& operator=(const CoverOffers&) = delete;

    /// Lists `game`'s covers, dropping a listing still under way.
    void list(const library::Game& game);
    /// Keeps `cover` as game `gameId`'s chosen cover.
    void choose(const std::string& gameId, const SteamGridDbCover& cover);
    /// Keeps game `gameId`'s own cover as its choice.
    void chooseOwn(const std::string& gameId);

    /// The listing as it stands, when it changed since the last call.
    [[nodiscard]] std::optional<CoverListing> takeListing();
    /// The choices that ended since the last call.
    [[nodiscard]] std::vector<CoverChoice> takeChoices();

  private:
    struct List {
        library::Game game;
        std::uint64_t generation{0};
    };
    struct Choose {
        std::string gameId;
        std::optional<SteamGridDbCover> cover;
    };
    using Work = std::variant<List, Choose>;

    void run(const std::stop_token& stop);
    void perform(const List& list);
    void perform(const Choose& choose);
    [[nodiscard]] bool current(std::uint64_t generation) const;
    void publish(const CoverListing& listing, std::uint64_t generation);

    const CoverOverrides& overrides_;
    const SteamGridDb& steamGridDb_;
    std::filesystem::path thumbDir_;
    net::WebClient web_;

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::vector<Work> queue_;
    std::uint64_t generation_{0};
    std::optional<CoverListing> listing_;
    std::vector<CoverChoice> choices_;
    /// Last, so the worker stops before anything it reads is destroyed.
    std::jthread worker_;
};

} // namespace opensu::artwork
