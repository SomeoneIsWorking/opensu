#include "game.hpp"

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>

namespace opensu::library {
namespace {

using Clock = std::chrono::system_clock;

std::chrono::system_clock::time_point playedAt(const Game& game) {
    return game.lastPlayed.value_or(Clock::time_point{});
}

/// Drops repeated ids, keeping the first occurrence.
std::vector<Game> dedupe(std::vector<Game> games) {
    std::vector<Game> out;
    out.reserve(games.size());
    std::unordered_set<std::string> seen;
    seen.reserve(games.size());
    for (Game& game : games) {
        if (game.id.empty() || !seen.insert(game.id).second) {
            continue;
        }
        out.push_back(std::move(game));
    }
    return out;
}

} // namespace

std::string_view label(Source source) {
    switch (source) {
    case Source::Steam:
        return "Steam";
    case Source::Epic:
        return "Epic";
    case Source::Gog:
        return "GOG";
    case Source::Rom:
        return "Emulator";
    }
    return "Unknown";
}

void Catalog::add(std::unique_ptr<Provider> provider) {
    if (provider != nullptr) {
        providers_.push_back(std::move(provider));
    }
}

std::vector<std::unique_ptr<Provider>> Catalog::release() && {
    return std::move(providers_);
}

SourceListing readProvider(Provider& provider) {
    SourceListing listing{.status = SourceStatus{.source = provider.source()}, .games = {}};
    try {
        listing.games = provider.list();
    } catch (const SourceAbsent& absent) {
        listing.status.availability = Availability::Absent;
        listing.status.detail = absent.what();
    } catch (const std::exception& error) {
        listing.status.availability = Availability::Attention;
        listing.status.detail = error.what();
    }
    return listing;
}

CatalogSnapshot assemble(const std::vector<Source>& sources,
                         const std::vector<SourceListing>& listings) {
    CatalogSnapshot snapshot;
    std::vector<Game> all;
    for (const Source source : sources) {
        const auto found = std::ranges::find(listings, source, [](const SourceListing& listing) {
            return listing.status.source;
        });
        if (found == listings.end()) {
            snapshot.sources.push_back(SourceStatus{
                .source = source, .availability = Availability::Loading, .detail = {}});
            continue;
        }
        snapshot.sources.push_back(found->status);
        all.insert(all.end(), found->games.begin(), found->games.end());
    }
    snapshot.games = dedupe(std::move(all));
    order(snapshot.games);
    return snapshot;
}

void order(std::vector<Game>& games) {
    std::ranges::stable_sort(games, [](const Game& a, const Game& b) {
        if (a.installed != b.installed) {
            return a.installed;
        }
        const Clock::time_point at = playedAt(a);
        const Clock::time_point bt = playedAt(b);
        if (at != bt) {
            return at > bt;
        }
        return a.title < b.title;
    });
}

} // namespace opensu::library
