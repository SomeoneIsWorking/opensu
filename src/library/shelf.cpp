#include "shelf.hpp"

#include <algorithm>
#include <utility>

#include "rom_systems.hpp"

namespace iideck::library {
namespace {

bool isRomOf(const Game& game, std::string_view system) {
    return game.source == Source::Rom && game.sourceId == system;
}

} // namespace

std::vector<Console> consoles(const std::vector<Game>& games) {
    std::vector<Console> out;
    for (const roms::RomSystem& system : roms::romSystems()) {
        const auto count =
            static_cast<std::size_t>(std::ranges::count_if(games, [&system](const Game& game) {
                return isRomOf(game, system.key);
            }));
        if (count > 0) {
            out.push_back(Console{std::string{system.key}, std::string{system.label}, count, {}});
        }
    }
    return out;
}

std::vector<ShelfItem> homeShelf(const std::vector<Game>& games) {
    std::vector<ShelfItem> shelf;
    for (Console& console : consoles(games)) {
        shelf.emplace_back(std::move(console));
    }
    for (const Game& game : games) {
        if (game.source != Source::Rom) {
            shelf.emplace_back(game);
        }
    }
    return shelf;
}

std::vector<ShelfItem> consoleShelf(const std::vector<Game>& games, std::string_view system) {
    std::vector<ShelfItem> shelf;
    for (const Game& game : games) {
        if (isRomOf(game, system)) {
            shelf.emplace_back(game);
        }
    }
    return shelf;
}

std::vector<ShelfItem> ShelfBrowser::shelf(const std::vector<Game>& games) {
    if (console_) {
        std::vector<ShelfItem> roms = consoleShelf(games, console_->system);
        if (!roms.empty()) {
            return roms;
        }
        console_.reset();
    }
    return homeShelf(games);
}

void ShelfBrowser::open(const Console& console, std::size_t homeFocus) {
    console_ = console;
    homeFocus_ = homeFocus;
}

std::optional<std::size_t> ShelfBrowser::back() {
    if (!console_) {
        return std::nullopt;
    }
    console_.reset();
    return homeFocus_;
}

} // namespace iideck::library
