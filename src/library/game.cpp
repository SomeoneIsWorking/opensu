#include "game.hpp"

#include <algorithm>
#include <memory>
#include <ranges>
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
    for (Game& game : games) {
        if (game.id.empty()) {
            continue;
        }
        if (std::ranges::any_of(out, [&game](const Game& seen) {
                return seen.id == game.id;
            })) {
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

CatalogSnapshot Catalog::refresh() {
    CatalogSnapshot snapshot;
    std::vector<Game> all;
    for (const std::unique_ptr<Provider>& provider : providers_) {
        SourceStatus status{.source = provider->source()};
        try {
            std::vector<Game> found = provider->list();
            all.insert(all.end(), std::make_move_iterator(found.begin()),
                       std::make_move_iterator(found.end()));
        } catch (const SourceAbsent& absent) {
            status.availability = Availability::Absent;
            status.detail = absent.what();
        } catch (const std::exception& error) {
            status.availability = Availability::Attention;
            status.detail = error.what();
        }
        snapshot.sources.push_back(std::move(status));
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
