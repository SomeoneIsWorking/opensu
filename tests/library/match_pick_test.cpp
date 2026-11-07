// The match the player picks for a game's artwork, kept at the game's setting layer: a store game
// or an added one takes a Steam app, a ROM a libretro box art title, and the source's own art
// gives way to the match's.
#include "library/match_pick.hpp"

#include <cstdio>
#include <vector>

#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu::library;
using opensu::settings::Layer;
using opensu::settings::SettingLayers;
using opensu::test::expect;

Game gameOf(Source source, std::string id, std::string sourceId) {
    Game game;
    game.id = std::move(id);
    game.source = source;
    game.sourceId = std::move(sourceId);
    game.artwork = "/art/own.jpg";
    game.artworkWide = "/art/wide.jpg";
    return game;
}

void storeGamesTakeASteamApp() {
    std::vector<Game> games{gameOf(Source::Gog, "gog:1", "1"), gameOf(Source::Epic, "epic:a", "a"),
                            gameOf(Source::Steam, "steam:5", "5"),
                            gameOf(Source::Own, "own:2", "2")};
    games[3].artworkKey = "400";
    SettingLayers layers;
    for (const Game& game : games) {
        expect(matchKindOf(game) == MatchKind::SteamApp, "a store or added game takes a Steam app");
        layers.set(Layer{Layer::Kind::Game, game.id}, matchKey, "620");
    }
    applyMatchPicks(layers, games);
    for (const Game& game : games) {
        expect(game.artworkKey == "620" && game.artwork.empty() && game.artworkWide.empty(),
               "the pick is the artwork key and the source's own art gives way");
    }
}

void anAddedGameTakesBoxArt() {
    std::vector<Game> games{gameOf(Source::Own, "own:2", "2"), gameOf(Source::Gog, "gog:1", "1")};
    games[0].artworkKey = "400";
    SettingLayers layers;
    for (const Game& game : games) {
        layers.set(Layer{Layer::Kind::Game, game.id}, matchKey, "box:psx/Dino Crisis 2 (USA)");
    }
    applyMatchPicks(layers, games);
    expect(games[0].boxArt == "psx/Dino Crisis 2 (USA)" && games[0].artworkKey.empty() &&
               games[0].artwork.empty(),
           "an added game shows the box art picked, in place of its Steam app");
    expect(games[1].boxArt.empty(), "a store game has no box art to pick");
}

void aHostSaysWhatItsGameCanBeMatchedTo() {
    Game rom = gameOf(Source::Rom, "rom:/r/Mario.sfc", "snes");
    Game added = gameOf(Source::Own, "own:1", "");
    Game remote = gameOf(Source::Remote, "lan:host:rom", "rom:/r/Mario.sfc");
    expect(matchKindOf(remote) == MatchKind::None, "a host that says nothing: no match");
    remote.matchAs = matchTargetOf(rom);
    expect(matchKindOf(remote) == MatchKind::LibretroTitle && matchSystemOf(remote) == "snes",
           "a host's ROM takes a box art title of its system");
    remote.matchAs = matchTargetOf(added);
    expect(matchKindOf(remote) == MatchKind::SteamApp && matchedAsAdded(remote),
           "a host's added game takes what an added game takes");
    remote.matchAs = matchTargetOf(gameOf(Source::Steam, "steam:1", "1"));
    expect(matchKindOf(remote) == MatchKind::SteamApp && !matchedAsAdded(remote),
           "a host's store game takes a Steam app");
}

void aRomTakesABoxArtTitle() {
    std::vector<Game> games{gameOf(Source::Rom, "rom:/r/Mario.sfc", "snes"),
                            gameOf(Source::Rom, "rom:/r/Kirby.nsp", "switch")};
    games[0].artworkKey = "Mario (U)";
    expect(matchKindOf(games[0]) == MatchKind::LibretroTitle, "a ROM takes a box art title");
    expect(matchKindOf(games[1]) == MatchKind::None, "a system libretro has no box art for: none");
    SettingLayers layers;
    layers.set(Layer{Layer::Kind::Game, games[0].id}, matchKey, "Super Mario World (USA)");
    layers.set(Layer{Layer::Kind::Game, games[1].id}, matchKey, "Kirby");
    applyMatchPicks(layers, games);
    expect(games[0].artworkKey == "Super Mario World (USA)" && games[0].artwork.empty(),
           "the ROM looks its box art up by the title picked");
    expect(games[1].artworkKey.empty() && games[1].artwork == "/art/own.jpg",
           "a pick that cannot apply changes nothing");
}

void aPickThatIsNoAppIdIsNone() {
    std::vector<Game> games{gameOf(Source::Gog, "gog:1", "1")};
    SettingLayers layers;
    layers.set(Layer{Layer::Kind::Game, "gog:1"}, matchKey, "Not an app");
    applyMatchPicks(layers, games);
    expect(games[0].artworkKey.empty() && games[0].artwork == "/art/own.jpg",
           "a store game's pick is only ever a Steam app id");
}

} // namespace

int main() try {
    storeGamesTakeASteamApp();
    anAddedGameTakesBoxArt();
    aRomTakesABoxArtTitle();
    aHostSaysWhatItsGameCanBeMatchedTo();
    aPickThatIsNoAppIdIsNone();
    std::printf("match_pick: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
