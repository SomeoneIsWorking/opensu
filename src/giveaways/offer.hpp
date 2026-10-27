// offer — one free-to-keep offer from a store: what it is, where its page and art are, when it
// starts and ends. The stores' parsers make them; the cache keeps them as JSON between runs.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "library/game.hpp"

namespace opensu::giveaways {

/// Unix time in seconds.
using Seconds = std::int64_t;

enum class OfferKind : std::uint8_t {
    Game,
    /// Content for a game, which the base game is needed for.
    Dlc,
};

struct Offer {
    library::Source store{library::Source::Steam};
    /// The store's id: a Steam app id, an Epic offer id, a GOG product id (or, for GOG's
    /// giveaway when the catalogue does not know it, its slug).
    std::string id;
    std::string title;
    OfferKind kind{OfferKind::Game};
    /// The store's wide image.
    std::string artUrl;
    /// The page Claim opens.
    std::string pageUrl;
    /// When it is free from and until; 0 when the store does not say.
    Seconds starts{0};
    Seconds ends{0};

    bool operator==(const Offer&) const = default;
};

/// "steam:123", "epic:<offer id>", "gog:<id>": unique across stores.
[[nodiscard]] std::string key(const Offer& offer);

/// Whether the offer is not free yet at `now`.
[[nodiscard]] bool upcoming(const Offer& offer, Seconds now) noexcept;

/// Whether the offer has ended at `now`.
[[nodiscard]] bool ended(const Offer& offer, Seconds now) noexcept;

/// An ISO 8601 date and time as the stores write it ("2026-10-29T15:00:00.000Z", an offset
/// "+01:00" or none for UTC), or nothing.
[[nodiscard]] std::optional<Seconds> parseIsoTime(std::string_view text);

/// The offers as the cache keeps them, and back; an entry that does not read is left out.
[[nodiscard]] std::string toJson(const std::vector<Offer>& offers, Seconds fetched);
struct Cached {
    std::vector<Offer> offers;
    Seconds fetched{0};
};
[[nodiscard]] std::optional<Cached> fromJson(std::string_view text);

} // namespace opensu::giveaways
