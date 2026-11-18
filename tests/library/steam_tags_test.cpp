// Steam tags: which games are Steam apps, their tags asked of a stand-in GetItems in the
// background and kept in a file, and the genre layout Auto takes from them.
#include "library/steam_tags.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "settings/video_settings.hpp"
#include "support/loopback_server.hpp"
#include "support/test_main.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::Game;
using opensu::library::Source;
using opensu::library::SteamTags;
using opensu::settings::TouchLayout;
using opensu::test::LoopbackServer;
using opensu::test::reply;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

Game steamGame(const std::string& app) {
    Game game;
    game.source = Source::Steam;
    game.id = "steam:" + app;
    return game;
}

void testAppOf() {
    using opensu::library::steamAppOf;
    expect(steamAppOf(steamGame("730")) == 730U, "a Steam game is its app");
    Game remote;
    remote.source = Source::Remote;
    remote.remote.source = "steam";
    remote.sourceId = "steam:1145360";
    expect(steamAppOf(remote) == 1145360U, "a Steam game on another openSU is its app there");
    remote.remote.source = "rom";
    remote.sourceId = "rom:gba:x";
    expect(!steamAppOf(remote), "a ROM over there is none");
    expect(!steamAppOf(steamGame("x7")), "an id that is no number is none");
}

void testFetchAndKeep(const fs::path& dir) {
    std::atomic<int> asked{0};
    LoopbackServer steam{[&asked](const lucent::http::Request& request) {
        ++asked;
        const bool both = request.target.find("730") != std::string::npos &&
                          request.target.find("1145360") != std::string::npos &&
                          request.target.find("include_tag_count") != std::string::npos;
        return both ? reply(200, R"({"response":{"store_items":[
            {"appid":730,"success":1,"name":"Counter-Strike 2","tagids":[1663,1774,3859]},
            {"appid":1145360,"success":1,"name":"Hades","tagids":[42804,3959]}]}})")
                    : reply(404, "{}");
    }};
    const fs::path file = dir / "steam-tags.json";
    {
        SteamTags tags{file, steam.base() + "/items"};
        tags.want({steamGame("730"), steamGame("1145360"), Game{}});
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{20};
        while (!tags.of(1145360) && std::chrono::steady_clock::now() < until) {
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        expect(tags.of(730) == std::vector<std::uint32_t>{1663, 1774, 3859},
               "an app's tags arrive in their order");
        expect(tags.of(1145360) == std::vector<std::uint32_t>{42804, 3959},
               "both apps in one request");
        tags.want({steamGame("730")});
    }
    expect(asked == 1, "an app known is not asked again");
    SteamTags again{file, steam.base() + "/items"};
    expect(again.of(730) == std::vector<std::uint32_t>{1663, 1774, 3859},
           "the tags are kept for the next run");
}

void testGenres() {
    using opensu::settings::touchLayoutOfTags;
    const std::vector<std::uint32_t> counterStrike{1663, 1774, 3859};
    const std::vector<std::uint32_t> raceThenShoot{699, 1663};
    const std::vector<std::uint32_t> celeste{3877, 5379};
    const std::vector<std::uint32_t> topDown{4637, 1774};
    expect(touchLayoutOfTags(counterStrike) == TouchLayout::Shooter, "an FPS takes the Shooter");
    expect(touchLayoutOfTags(raceThenShoot) == TouchLayout::Racing, "the most voted genre wins");
    expect(touchLayoutOfTags(celeste) == TouchLayout::Platformer, "a precision platformer");
    expect(!touchLayoutOfTags(topDown), "a top-down shooter aims with no mouse, so none");
}

} // namespace

int main() try {
    const fs::path dir = fs::path{OPENSU_TEST_SCRATCH} / ("steam-tags-" + std::to_string(getpid()));
    fs::remove_all(dir);
    testAppOf();
    testFetchAndKeep(dir);
    testGenres();
    fs::remove_all(dir);
    std::printf("steam_tags: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
