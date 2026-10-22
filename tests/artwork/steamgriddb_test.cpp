// SteamGridDB's API, against a fake API: covers of a shape by Steam app id and by title, the key
// sent as a bearer token to the API only, and what a refused key or a missing API come to.
#include "steamgriddb.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string>

#include "net/web_client.hpp"
#include "support/steamgriddb_server.hpp"

namespace {

using opensu::artwork::CoverShape;
using opensu::artwork::SteamGridDb;
using Status = SteamGridDb::Covers::Status;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

constexpr const char* key = "0123456789abcdef0123456789abcdef";

void testListsCoversByShape() {
    opensu::test::SteamGridDbServer api{key};
    api.putSteam("620", {"portal2-a.png", "a"});
    api.putSteam("620", {"portal2-b.png", "b"});
    api.putSteam("620", {"portal2-wide.png", "w", 920, 430});
    api.putGame("Celeste", 7, {"celeste.png", "c"});
    SteamGridDb source{api.api()};
    source.setKey(key);
    const opensu::net::WebClient web;

    const SteamGridDb::Covers portal =
        source.covers(web, {"620", "Portal 2"}, CoverShape::Portrait);
    expect(portal.status == Status::Listed && portal.covers.size() == 2 &&
               portal.covers[0].url.ends_with("/img/portal2-a.png") &&
               portal.covers[0].width == 600 && portal.covers[0].height == 900 &&
               portal.covers[0].id == 1,
           "a Steam game's covers are listed by its app id, best first");
    expect(source.covers(web, {"620", ""}, CoverShape::Landscape).covers.size() == 1,
           "a landscape tile gets the wide cover only");
    expect(source.covers(web, {"620", ""}, std::nullopt).covers.size() == 3,
           "the picker gets every shape");
    expect(
        source.covers(web, {"", "Celeste"}, std::nullopt).covers.at(0).url.ends_with("celeste.png"),
        "a game without an app id is found by its title");
    expect(source.covers(web, {"999", "Celeste"}, CoverShape::Portrait).covers.size() == 1,
           "an app id SteamGridDB does not know falls back to the title");
    const SteamGridDb::Covers none = source.covers(web, {"", "Nothing Here"}, std::nullopt);
    expect(none.status == Status::Listed && none.covers.empty(), "an unknown title lists nothing");

    std::string portraitQuery;
    std::string anyQuery = "unset";
    for (const opensu::test::SteamGridDbServer::Seen& seen : api.seen()) {
        expect(seen.authorized, "every API call carries the key");
        if (seen.path == "/api/v2/grids/steam/620" && portraitQuery.empty()) {
            portraitQuery = seen.query;
        }
        if (seen.path == "/api/v2/grids/game/7" && anyQuery == "unset") {
            anyQuery = seen.query;
        }
    }
    expect(portraitQuery.find("dimensions=600x900") != std::string::npos,
           "a portrait tile asks for Steam's portrait sizes");
    expect(anyQuery.empty(), "the picker asks for every shape");
    expect(!source.refused(), "an accepted key is not refused");
}

void testShapes() {
    expect(opensu::artwork::coverShapeOf(600, 900) == CoverShape::Portrait, "2:3 is portrait");
    expect(opensu::artwork::coverShapeOf(460, 215) == CoverShape::Landscape, "a capsule is wide");
    expect(opensu::artwork::coverShapeOf(512, 512) == CoverShape::Square, "1:1 is square");
    expect(opensu::artwork::coverShapeOf(640, 560) == CoverShape::Square,
           "a near-square box is square");
}

void testRefusedKey() {
    opensu::test::SteamGridDbServer api{key};
    api.putSteam("620", {"portal2-grid.png", "grid"});
    SteamGridDb source{api.api()};
    source.setKey("ffffffffffffffffffffffffffffffff");
    const opensu::net::WebClient web;
    expect(source.covers(web, {"620", "Portal 2"}, CoverShape::Portrait).status == Status::Refused,
           "a wrong key is refused");
    expect(source.refused(), "the refusal is remembered for the settings");
    source.setKey(key);
    expect(!source.refused(), "a new key clears it");
    expect(source.covers(web, {"620", ""}, CoverShape::Portrait).covers.size() == 1,
           "and finds the cover");
}

void testNoKeyAsksNothing() {
    opensu::test::SteamGridDbServer api{key};
    SteamGridDb source{api.api()};
    const opensu::net::WebClient web;
    expect(!source.connected(), "no key is not connected");
    expect(source.covers(web, {"620", "Portal 2"}, std::nullopt).status == Status::Disconnected,
           "without a key there is nothing");
    expect(api.seen().empty(), "and nothing is asked");
    source.setKey(key);
    source.setKey({});
    expect(!source.connected(), "an empty key disconnects");
}

void testUnreachableApi() {
    std::string gone;
    {
        const opensu::test::SteamGridDbServer api{key};
        gone = api.api();
    }
    SteamGridDb source{gone};
    source.setKey(key);
    const opensu::net::WebClient web;
    const SteamGridDb::Covers found = source.covers(web, {"620", ""}, std::nullopt);
    expect(found.status == Status::Unreachable && !found.error.empty(),
           "an API that does not answer is unreachable");
    expect(found.error.find(key) == std::string::npos, "the error never holds the key");
}

} // namespace

int main() {
    try {
        testShapes();
        testListsCoversByShape();
        testRefusedKey();
        testNoKeyAsksNothing();
        testUnreachableApi();
        std::printf("steamgriddb: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}
