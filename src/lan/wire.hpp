// wire — the LAN protocol's messages as the JSON bodies both sides send, the one place each is
// written and read. Keys and nonces travel as lower-case hex; anything malformed reads as nothing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "peer_crypto.hpp"

namespace opensu::lan::wire {

/// A string field of a document, empty when it is absent or not a string.
[[nodiscard]] std::string textField(const nlohmann::json& document, const char* key);

/// `GET /lan/hello`: who answers, the one thing an unpaired openSU learns.
struct Hello {
    std::string id;
    std::string name;
};
[[nodiscard]] std::string write(const Hello& hello);
[[nodiscard]] std::optional<Hello> readHello(std::string_view body);

/// `POST /lan/pair`: a guest's request, committed to its key.
struct PairRequest {
    std::string id;
    std::string name;
    crypto::Bytes commitment;
};
[[nodiscard]] std::string write(const PairRequest& request);
[[nodiscard]] std::optional<PairRequest> readPairRequest(std::string_view body);

/// The host's answer to it: its key and nonce for this pairing.
struct PairOffer {
    crypto::Bytes key;
    crypto::Bytes nonce;
};
[[nodiscard]] std::string write(const PairOffer& offer);
[[nodiscard]] std::optional<PairOffer> readPairOffer(std::string_view body);

/// `POST /lan/pair/reveal`: the guest's key and nonce.
struct PairReveal {
    std::string id;
    crypto::Bytes key;
    crypto::Bytes nonce;
};
[[nodiscard]] std::string write(const PairReveal& reveal);
[[nodiscard]] std::optional<PairReveal> readPairReveal(std::string_view body);

/// `POST /lan/pair/status`: which guest asks, and the host's word back.
struct PairPoll {
    std::string id;
};
[[nodiscard]] std::string write(const PairPoll& poll);
[[nodiscard]] std::optional<PairPoll> readPairPoll(std::string_view body);

struct PairVerdict {
    std::string verdict;
    std::string signature;
};
[[nodiscard]] std::string write(const PairVerdict& verdict);
[[nodiscard]] std::optional<PairVerdict> readPairVerdict(std::string_view body);

/// One game of `GET /lan/library`.
struct ListedGame {
    std::string id;
    std::string title;
    /// Its cover's token (`isCoverToken`); empty when it has none.
    std::string cover;

    bool operator==(const ListedGame&) const = default;
};

/// A cover token is this many bytes, sent as hex.
inline constexpr std::size_t coverTokenBytes = 16;

/// Whether `text` can be a cover token: `coverTokenBytes` as lower-case hex.
[[nodiscard]] bool isCoverToken(std::string_view text) noexcept;

/// `{"games":[{"id","title","cover"}]}`.
[[nodiscard]] std::string write(const std::vector<ListedGame>& games);
/// A listing read back; nothing when `body` is not one. Games without an id or a title are left
/// out, and covers that are not tokens dropped.
[[nodiscard]] std::optional<std::vector<ListedGame>> readListing(std::string_view body);

/// The routes a paired peer that shares saves asks.
inline constexpr std::string_view saveStatusRoute = "/lan/saves/status";
inline constexpr std::string_view saveGetRoute = "/lan/saves/get";
inline constexpr std::string_view saveNewsRoute = "/lan/saves/news";
inline constexpr std::string_view saveSyncedRoute = "/lan/saves/synced";

/// `POST /lan/saves/status`, `/get`, `/news` and `/synced`: which game's saves, by its
/// `library::saves::SaveKey`.
struct SaveAsk {
    std::string system;
    std::string game;
    /// `/news`: the player picked the sender's saves over the receiver's.
    bool picked{false};
    /// `/synced`: the hash both now hold, and the counter the sender holds it as.
    std::string hash;
    std::uint64_t counter{0};
};
[[nodiscard]] std::string write(const SaveAsk& ask);
/// Nothing when `body` is not one, or its system or game could step out of a folder.
[[nodiscard]] std::optional<SaveAsk> readSaveAsk(std::string_view body);

/// The answer to `/lan/saves/status`: the version of the game's saves there, their size and newest
/// change (Unix seconds, 0 for none).
struct SaveStatus {
    std::uint64_t counter{0};
    std::string hash;
    std::uint64_t bytes{0};
    std::int64_t modified{0};

    bool operator==(const SaveStatus&) const = default;
};
[[nodiscard]] std::string write(const SaveStatus& status);
[[nodiscard]] std::optional<SaveStatus> readSaveStatus(std::string_view body);

} // namespace opensu::lan::wire
