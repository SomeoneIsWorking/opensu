#include "match_pick.hpp"

#include "rom_systems.hpp"

namespace opensu::library {

MatchKind matchKindOf(const Game& game) {
    switch (game.source) {
    case Source::Steam:
    case Source::Epic:
    case Source::Gog:
    case Source::Own:
        return MatchKind::SteamApp;
    case Source::Rom: {
        const roms::RomSystem* system = roms::systemByKey(game.sourceId);
        return system != nullptr && !system->libretroName.empty() ? MatchKind::LibretroTitle
                                                                  : MatchKind::None;
    }
    }
    return MatchKind::None;
}

void applyMatchPicks(const settings::SettingLayers& layers, std::vector<Game>& games) {
    for (Game& game : games) {
        const std::string* pick =
            layers.at(settings::Layer{settings::Layer::Kind::Game, game.id}, matchKey);
        if (pick == nullptr || pick->empty()) {
            continue;
        }
        const MatchKind kind = matchKindOf(game);
        if (kind == MatchKind::None || (kind == MatchKind::SteamApp && !isSteamAppId(*pick))) {
            continue;
        }
        game.artworkKey = *pick;
        game.artwork.clear();
        game.artworkWide.clear();
    }
}

} // namespace opensu::library
