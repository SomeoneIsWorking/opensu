// The covers SteamGridDB puts in place of a game's own, against a fake API: a portrait cover gets a
// square one fitted for a square tile while the own file stays, a square cover is left alone, a
// chosen cover or a kept own cover wins over fitting, a game SteamGridDB has nothing for is not
// asked again, and "Choose artwork" lists every shape with thumbnails and keeps the pick.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "artwork_store.hpp"
#include "cover_fitter.hpp"
#include "cover_offers.hpp"
#include "cover_overrides.hpp"
#include "gpu2d/image/png_encoder.hpp"
#include "library/game.hpp"
#include "support/scratch_dir.hpp"
#include "support/steamgriddb_server.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::artwork::CoverFitter;
using opensu::artwork::CoverListing;
using opensu::artwork::CoverOffers;
using opensu::artwork::CoverOverrides;
using opensu::artwork::CoverShape;
using opensu::artwork::FittedCover;
using opensu::artwork::SteamGridDb;
using opensu::test::SteamGridDbServer;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

constexpr const char* key = "0123456789abcdef0123456789abcdef";

std::string png(int width, int height) {
    const std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width * height) * 4U, 200);
    std::string error;
    std::string bytes = gpu2d::image::encodePng(rgba, static_cast<unsigned>(width),
                                                static_cast<unsigned>(height), error);
    expect(!bytes.empty(), "the fixture encodes");
    return bytes;
}

std::string readFile(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

opensu::library::Game steamGame(const std::string& appId, const std::string& title,
                                const fs::path& cover) {
    opensu::library::Game game;
    game.id = "steam:" + appId;
    game.title = title;
    game.source = opensu::library::Source::Steam;
    game.sourceId = appId;
    game.artwork = cover;
    return game;
}

void waitUntil(const std::function<bool()>& done, const char* what) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (!done()) {
        expect(std::chrono::steady_clock::now() < until, what);
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
}

void writeFile(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << bytes;
}

struct Rig {
    explicit Rig(const char* name)
        : scratch{OPENSU_TEST_SCRATCH, name},
          overrides{scratch.path() / "data" / "chosen", scratch.path() / "cache" / "fitted"} {
        steamGridDb.setKey(key);
    }

    opensu::test::ScratchDir scratch;
    SteamGridDbServer api{key};
    SteamGridDb steamGridDb{api.api()};
    CoverOverrides overrides;
    opensu::artwork::ArtworkStore store{scratch.path() / "cache" / "artwork"};
};

void testFitsAPortraitCoverToASquareTile() {
    Rig rig{"cover-fit"};
    const std::string square = png(64, 64);
    rig.api.putSteam("620", {"portal2-square.png", square, 512, 512});
    rig.api.putSteam("620", {"portal2-tall.png", png(40, 60), 600, 900});
    rig.api.putSteam("400", {"portal-square.png", square, 512, 512});
    const fs::path tall = rig.scratch.path() / "own" / "620.png";
    const fs::path boxy = rig.scratch.path() / "own" / "400.png";
    const std::string tallBytes = png(40, 60);
    writeFile(tall, tallBytes);
    writeFile(boxy, png(50, 50));
    rig.api.putTitle("Nothing Here", 9);
    const fs::path nothing = rig.scratch.path() / "own" / "nothing.png";
    writeFile(nothing, tallBytes);
    opensu::library::Game unknown = steamGame("1", "Nothing Here", nothing);

    CoverFitter fitter{rig.overrides, rig.steamGridDb, rig.store};
    fitter.request({steamGame("620", "Portal 2", tall), steamGame("400", "Portal", boxy), unknown},
                   CoverShape::Square);
    std::vector<FittedCover> fitted;
    waitUntil(
        [&] {
            for (FittedCover& one : fitter.take()) {
                fitted.push_back(std::move(one));
            }
            return fitter.idle();
        },
        "the fitter settles");
    expect(fitted.size() == 1 && fitted[0].id == "steam:620" && readFile(fitted[0].cover) == square,
           "the portrait cover gets SteamGridDB's square one");
    expect(rig.overrides.shown("steam:620") == fitted[0].cover, "the fitted cover is shown");
    expect(readFile(tall) == tallBytes, "the game's own cover stays on disk");
    expect(rig.overrides.shown("steam:400").empty(), "a cover that fits is left alone");
    expect(rig.overrides.shown("steam:1").empty() &&
               !rig.overrides.wantsFit("steam:1", opensu::artwork::ArtworkStore::Clock::now()),
           "a game SteamGridDB has no square cover for is not asked again soon");
    bool squareAsked = false;
    for (const SteamGridDbServer::Seen& seen : rig.api.seen()) {
        squareAsked = squareAsked || (seen.path == "/api/v2/grids/steam/620" &&
                                      seen.query.find("512x512") != std::string::npos);
    }
    expect(squareAsked, "the square shape is asked for");

    std::vector<opensu::library::ShelfItem> shelf{steamGame("620", "Portal 2", tall),
                                                  steamGame("400", "Portal", boxy)};
    rig.overrides.apply(shelf);
    expect(std::get<opensu::library::Game>(shelf[0]).artwork == fitted[0].cover &&
               std::get<opensu::library::Game>(shelf[1]).artwork == boxy,
           "a shelf shows the fitted cover in place of the own one only where there is one");

    std::string error;
    expect(rig.overrides.chooseOwn("steam:620", error), "the own cover can be chosen");
    expect(rig.overrides.shown("steam:620").empty(), "a kept own cover wins over the fitted one");
    expect(!rig.overrides.wantsFit("steam:620", opensu::artwork::ArtworkStore::Clock::now()),
           "and is not fitted again");
}

opensu::library::Game gogGame(const std::string& id, const std::string& title) {
    opensu::library::Game game;
    game.id = "gog:" + id;
    game.title = title;
    game.source = opensu::library::Source::Gog;
    game.sourceId = id;
    return game;
}

void testFitsAGameWhoseSourceHasNoCover() {
    Rig rig{"cover-fit-coverless"};
    const std::string square = png(64, 64);
    rig.api.putGame("Missed Game", 11, {"missed-square.png", square, 512, 512});
    rig.api.putGame("Pending Game", 12, {"pending-square.png", png(64, 64), 512, 512});
    const opensu::library::Game missed = gogGame("1", "Missed Game");
    const opensu::library::Game pending = gogGame("2", "Pending Game");
    rig.store.recordMiss(missed);

    CoverFitter fitter{rig.overrides, rig.steamGridDb, rig.store};
    fitter.request({missed, pending}, CoverShape::Square);
    std::vector<FittedCover> fitted;
    waitUntil(
        [&] {
            for (FittedCover& one : fitter.take()) {
                fitted.push_back(std::move(one));
            }
            return fitter.idle();
        },
        "the fitter settles");
    expect(fitted.size() == 1 && fitted[0].id == "gog:1" && readFile(fitted[0].cover) == square,
           "a game its source has no cover for gets one by its title");
    expect(rig.overrides.shown("gog:2").empty(),
           "a game whose cover is still to be downloaded is left alone");
}

void testFittingNeedsAKey() {
    Rig rig{"cover-fit-nokey"};
    rig.steamGridDb.setKey("");
    const fs::path tall = rig.scratch.path() / "own" / "620.png";
    writeFile(tall, png(40, 60));
    CoverFitter fitter{rig.overrides, rig.steamGridDb, rig.store};
    fitter.request({steamGame("620", "Portal 2", tall)}, CoverShape::Square);
    waitUntil(
        [&] {
            return fitter.idle();
        },
        "the fitter settles");
    expect(fitter.take().empty() && rig.api.seen().empty(), "without a key nothing is asked");
}

void testChooseArtwork() {
    Rig rig{"cover-offers"};
    const std::string chosen = png(60, 90);
    rig.api.putSteam("620", {"portal2-tall.png", chosen, 600, 900});
    rig.api.putSteam("620", {"portal2-square.png", png(64, 64), 512, 512});
    rig.api.putSteam("620", {"portal2-wide.png", png(92, 43), 920, 430});
    const fs::path own = rig.scratch.path() / "own" / "620.png";
    writeFile(own, png(40, 60));
    CoverOffers offers{rig.overrides, rig.steamGridDb, rig.scratch.path() / "cache" / "thumbs"};
    offers.list(steamGame("620", "Portal 2", own));
    std::optional<CoverListing> listing;
    waitUntil(
        [&] {
            if (std::optional<CoverListing> next = offers.takeListing()) {
                listing = std::move(next);
            }
            return listing && listing->complete;
        },
        "the listing completes");
    expect(listing.has_value(), "a listing came");
    if (!listing) {
        return;
    }
    expect(listing->gameId == "steam:620" && listing->offers.size() == 3, "every shape is offered");
    bool thumbs = true;
    for (const opensu::artwork::CoverOffer& offer : listing->offers) {
        thumbs = thumbs && fs::is_regular_file(offer.thumb);
    }
    expect(thumbs, "each offer's thumbnail is on disk");

    offers.choose("steam:620", listing->offers[0].cover);
    std::vector<opensu::artwork::CoverChoice> choices;
    waitUntil(
        [&] {
            for (opensu::artwork::CoverChoice& choice : offers.takeChoices()) {
                choices.push_back(std::move(choice));
            }
            return !choices.empty();
        },
        "the choice ends");
    expect(choices[0].kept && readFile(rig.overrides.shown("steam:620")) == chosen,
           "the picked cover is kept and shown");
    const CoverOverrides again{rig.scratch.path() / "data" / "chosen",
                               rig.scratch.path() / "cache" / "fitted"};
    expect(again.shown("steam:620") == rig.overrides.shown("steam:620"),
           "the choice outlives this run");
    expect(fs::exists(own), "the game's own cover stays on disk");

    offers.chooseOwn("steam:620");
    choices.clear();
    waitUntil(
        [&] {
            for (opensu::artwork::CoverChoice& choice : offers.takeChoices()) {
                choices.push_back(std::move(choice));
            }
            return !choices.empty();
        },
        "the own choice ends");
    expect(choices[0].kept && rig.overrides.shown("steam:620").empty() &&
               rig.overrides.keptOwn("steam:620"),
           "choosing the own cover back shows it again");
}

} // namespace

int main() {
    try {
        testFitsAPortraitCoverToASquareTile();
        testFitsAGameWhoseSourceHasNoCover();
        testFittingNeedsAKey();
        testChooseArtwork();
        std::puts("steamgriddb_covers: ok");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}
