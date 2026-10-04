#include "game.hpp"

#include <algorithm>
#include <memory>
#include <ranges>
#include <utility>

namespace iideck::library {
namespace {

using Clock = std::chrono::system_clock;

/// A title touched within this window is something to resume.
constexpr std::chrono::hours resumeWindow{24 * 7};

/// Past that, it is something you came back to rather than continue.
constexpr std::chrono::hours lingerWindow{24 * 90};

std::chrono::system_clock::time_point playedAt(const Game& game)
{
    return game.lastPlayed.value_or(Clock::time_point{});
}

/// Drops repeated ids, keeping the first occurrence.
std::vector<Game> dedupe(std::vector<Game> games)
{
    std::vector<Game> out;
    out.reserve(games.size());
    for (Game& game : games) {
        if (game.id.empty()) {
            continue;
        }
        if (std::ranges::any_of(out, [&game](const Game& seen) { return seen.id == game.id; })) {
            continue;
        }
        out.push_back(std::move(game));
    }
    return out;
}

} // namespace

std::string_view label(Source source)
{
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

void Catalog::add(std::unique_ptr<Provider> provider)
{
    if (provider != nullptr) {
        providers_.push_back(std::move(provider));
    }
}

std::vector<Game> Catalog::refresh(std::vector<std::string>& problems)
{
    std::vector<Game> all;
    for (const std::unique_ptr<Provider>& provider : providers_) {
        try {
            std::vector<Game> found = provider->list();
            all.insert(all.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
        } catch (const std::exception& error) {
            problems.push_back(std::string{label(provider->source())} + ": " + error.what());
        }
    }
    std::vector<Game> merged = dedupe(std::move(all));
    order(merged);
    annotate(merged);
    return merged;
}

void order(std::vector<Game>& games)
{
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

void annotate(std::vector<Game>& games)
{
    const Clock::time_point now = Clock::now();
    int resumed = 0;
    int lingering = 0;

    for (Game& game : games) {
        if (!game.installed) {
            game.size = TileSize::Square;
            game.badge = "Not installed";
        } else if (!game.lastPlayed) {
            game.size = TileSize::Square;
            game.hint = "Ready to play";
        } else {
            const auto age = now - *game.lastPlayed;
            if (age <= resumeWindow) {
                game.hint = "Jump back in!";
                if (resumed == 0) {
                    game.size = TileSize::Hero;
                } else if (resumed <= 2) {
                    game.size = TileSize::Wide;
                } else {
                    game.size = TileSize::Square;
                }
                ++resumed;
            } else if (age <= lingerWindow) {
                game.hint = "Been a while...";
                if (lingering == 0) {
                    game.size = TileSize::Wide;
                } else {
                    game.size = TileSize::Square;
                }
                ++lingering;
            } else {
                game.size = TileSize::Square;
                game.hint = "Been a while...";
            }
        }
        // A store badge on anything that is not already Steam, so the tile shows
        // where a title comes from.
        if (game.source != Source::Steam && game.badge.empty()) {
            game.badge = std::string{label(game.source)};
        }
    }
}

} // namespace iideck::library