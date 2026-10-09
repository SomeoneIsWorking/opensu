// The fetcher against a local server standing in for Steam's CDN and libretro-thumbnails.
#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "arcade_names.hpp"
#include "lucent/http.h"

namespace {

namespace fs = std::filesystem;
using opensu::artwork::ArtworkFetcher;
using opensu::artwork::ArtworkStore;
using opensu::artwork::Fetched;
using opensu::artwork::Stage;
using opensu::library::Game;
using opensu::library::Source;
using opensu::library::roms::NameDb;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

Game game(Source source, std::string id, std::string sourceId, std::string key = {}) {
    Game out;
    out.id = std::move(id);
    out.title = out.id;
    out.source = source;
    out.sourceId = std::move(sourceId);
    out.artworkKey = std::move(key);
    return out;
}

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

std::vector<Fetched> waitFor(ArtworkFetcher& fetcher) {
    std::vector<Fetched> all;
    for (int i = 0; i < 500 && !(fetcher.idle() && i > 0); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    for (Fetched& item : fetcher.take()) {
        if (item.saved) {
            all.push_back(std::move(item));
        }
    }
    return all;
}

// A listing in the real format, with the size and CRC-32 the test pins it by.
constexpr std::string_view arcadeDat =
    "clrmamepro (\n\tname \"Fixture\"\n)\n\n"
    "game (\n\tname \"Street Fighter II: The World Warrior (World 910522)\"\n\tyear \"1991\"\n"
    "\trom ( name sf2.zip size 3551819 crc B62D0BC7 )\n)\n";
constexpr opensu::artwork::DatPin arcadePin{"fbneo-split/FBNeo - Arcade Games.dat", 162,
                                            0x209290d5};

class FakeSources {
  public:
    FakeSources()
        : server_{lucent::http::ServerOptions{}, [this](const lucent::http::Request& request) {
                      return answer(request);
                  }} {
        expect(server_.start(), "the fake server starts");
    }

    [[nodiscard]] std::string base() const {
        return "http://127.0.0.1:" + std::to_string(server_.port());
    }
    /// Lets the held download of steam:777 answer.
    void release() {
        {
            const std::lock_guard lock{mutex_};
            released_ = true;
        }
        released_changed_.notify_all();
    }
    [[nodiscard]] std::vector<std::string> asked() const {
        const std::lock_guard lock{mutex_};
        return asked_;
    }

  private:
    lucent::http::Response answer(const lucent::http::Request& request) {
        const std::string path{request.path()};
        {
            const std::lock_guard lock{mutex_};
            asked_.push_back(path);
        }
        if (path == "/steam/777/library_600x900.jpg") {
            std::unique_lock lock{mutex_};
            released_changed_.wait(lock, [this] {
                return released_;
            });
            return lucent::http::Response::binary(200, "OK", "image/jpeg", "slow cover");
        }
        if (path == "/steam/440/header.jpg") {
            return lucent::http::Response::binary(200, "OK", "image/jpeg", "steam header");
        }
        if (path == "/epic/tall.jpg") {
            return lucent::http::Response::binary(200, "OK", "image/jpeg", "epic tall");
        }
        if (path == "/gogdb/1001") {
            return lucent::http::Response::json(
                200, "OK",
                R"({"game":{"vertical_cover":{"url_format":")" + base() +
                    R"(/gogimg/abc{formatter}.{ext}?namespace=gamesdb"}}})");
        }
        if (path == "/gogimg/abc_glx_vertical_cover.jpg") {
            return lucent::http::Response::binary(200, "OK", "image/jpeg", "gog portrait");
        }
        if (path == "/gogdb/1002") {
            return lucent::http::Response::json(200, "OK", R"({"game":{}})");
        }
        if (path == "/gogtile/def_196.jpg") {
            return lucent::http::Response::binary(200, "OK", "image/jpeg", "gog tile");
        }
        if (path == std::string{"/ldb/"} +
                        std::string{opensu::library::roms::libretroDatabaseRevision} +
                        "/metadat/fbneo-split/FBNeo%20-%20Arcade%20Games.dat") {
            return lucent::http::Response::text(200, "OK", std::string{arcadeDat});
        }
        if (path == "/libretro/Nintendo%20-%20GameCube/Named_Boxarts/") {
            return lucent::http::Response::text(
                200, "OK",
                R"(<a href="Super%20Mario%20Sunshine%20(USA).png">x</a>)"
                R"(<a href="Super%20Mario%20Sunshine%20(Europe).png">x</a>)");
        }
        if (path == "/libretro/Nintendo%20-%20GameCube/Named_Boxarts/"
                    "Super%20Mario%20Sunshine%20(USA).png") {
            return lucent::http::Response::binary(200, "OK", "image/png", "sunshine box");
        }
        return lucent::http::Response::text(404, "Not Found", "");
    }

    mutable std::mutex mutex_;
    std::condition_variable released_changed_;
    bool released_{false};
    std::vector<std::string> asked_;
    lucent::http::Server server_;
};

void testFetches(const fs::path& root) {
    FakeSources sources;
    ArtworkStore store{root};
    Game epic = game(Source::Epic, "epic:Owned", "Owned");
    epic.artworkUrl = sources.base() + "/epic/tall.jpg";
    Game gogPortrait = game(Source::Gog, "gog:1001", "1001");
    Game gogTile = game(Source::Gog, "gog:1002", "1002");
    gogTile.artworkUrl = sources.base() + "/gogtile/def";
    const std::vector<Game> games{
        game(Source::Steam, "steam:440", "440"),
        game(Source::Rom, "rom:sunshine", "gc", "Super Mario Sunshine"),
        game(Source::Rom, "rom:nothing", "gc", "No Such Game (USA)"),
        game(Source::Rom, "rom:switch", "switch", "Xenoblade Chronicles 2"),
        epic,
        gogPortrait,
        gogTile,
        game(Source::Gog, "gog:1003", "1003"),
    };
    {
        ArtworkFetcher fetcher{store,
                               {.libretro = sources.base() + "/libretro",
                                .steam = sources.base() + "/steam",
                                .gogdb = sources.base() + "/gogdb"}};
        fetcher.request(games, {}, {});
        const std::vector<Fetched> fetched = waitFor(fetcher);
        expect(fetched.size() == 5, "the Steam, Epic and GOG games and the matched ROM arrive");
        expect(read(store.pathFor(games[4])) == "epic tall",
               "an Epic game gets its key image from its URL");
        expect(read(store.pathFor(games[5])) == "gog portrait",
               "a GOG game gets the portrait cover gamesdb names");
        expect(read(store.pathFor(games[6])) == "gog tile",
               "a GOG game gamesdb has no cover for gets its library tile");
        expect(fs::exists(store.pathFor(games[7]).string() + ".miss"),
               "a GOG game with no cover anywhere is noted as a miss");
        expect(read(store.pathFor(games[0])) == "steam header",
               "a Steam game without a portrait gets its store header");
        expect(read(store.pathFor(games[1])) == "sunshine box",
               "a ROM gets the box art its title matches, USA first");
        expect(fs::exists(store.pathFor(games[2]).string() + ".miss"),
               "a ROM the listing lacks is noted as a miss");
        expect(store.pathFor(games[3]).empty(), "a system libretro lacks has no source");
    }

    std::vector<Game> again = games;
    store.apply(again);
    expect(again[0].artwork == store.pathFor(games[0]) && again[2].artwork.empty(),
           "the next start reads downloaded art from the store");
    const auto now = ArtworkStore::Clock::now();
    expect(!store.wanted(again[0], now) && !store.wanted(again[2], now) &&
               !store.wanted(again[7], now),
           "neither a kept image nor a recent miss is asked for again");
    expect(store.index("gc", now).has_value(), "the listing is kept for the next run");
}

void testArcadeNames(const fs::path& root) {
    FakeSources sources;
    ArtworkStore store{root / "artwork"};
    const NameDb names = NameDb::under(root);
    expect(arcadeDat.size() == arcadePin.size, "the fixture is the size it is pinned at");
    const std::vector<Game> games{game(Source::Rom, "rom:sf2", "arcade", "sf2")};
    {
        ArtworkFetcher fetcher{store,
                               {.libretro = sources.base() + "/libretro",
                                .libretroDatabase = sources.base() + "/ldb",
                                .arcadeDats = {arcadePin}},
                               names};
        fetcher.request(games, {}, {});
        const std::vector<Fetched> fetched = waitFor(fetcher);
        expect(std::ranges::any_of(fetched,
                                   [](const Fetched& item) {
                                       return item.kind == Fetched::Kind::Names;
                                   }),
               "the arcade names arrive");
    }
    expect(names.cached() &&
               names.load().at("sf2") == "Street Fighter II: The World Warrior (World 910522)",
           "the listing is kept parsed");
    {
        ArtworkFetcher fetcher{store,
                               {.libretro = sources.base() + "/libretro",
                                .libretroDatabase = sources.base() + "/ldb",
                                .arcadeDats = {arcadePin}},
                               names};
        fetcher.request(games, {}, {});
        expect(waitFor(fetcher).empty(), "kept names are not fetched again");
    }

    // A listing that differs from its pin is refused and nothing is kept.
    const NameDb refused = NameDb::under(root / "refused");
    ArtworkFetcher fetcher{store,
                           {.libretro = sources.base() + "/libretro",
                            .libretroDatabase = sources.base() + "/ldb",
                            .arcadeDats = {{arcadePin.path, arcadePin.size, 1}}},
                           refused};
    fetcher.request(games, {}, {});
    expect(waitFor(fetcher).empty() && !refused.cached(), "a listing off its pin is not kept");
}

void testUnreachable(const fs::path& root) {
    ArtworkStore store{root};
    // Port 9 is discard; nothing listens on loopback there.
    ArtworkFetcher fetcher{store, {"http://127.0.0.1:9/libretro", "http://127.0.0.1:9/steam"}};
    const Game steam = game(Source::Steam, "steam:1", "1");
    fetcher.request({steam, game(Source::Steam, "steam:2", "2")}, {}, {});
    expect(waitFor(fetcher).empty(), "nothing arrives from a source that cannot be reached");
    expect(!fs::exists(store.pathFor(steam).string() + ".miss"),
           "an unreachable source is not a miss, so the next run asks again");
}

template <class Ready> bool eventually(const Ready& ready) {
    for (int i = 0; i < 500; ++i) {
        if (ready()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    return false;
}

void testStages(const fs::path& root) {
    FakeSources sources;
    ArtworkStore store{root};
    const std::vector<Game> games{game(Source::Steam, "steam:777", "777"),
                                  game(Source::Steam, "steam:440", "440"),
                                  game(Source::Steam, "steam:404", "404")};
    ArtworkFetcher fetcher{store, {.steam = sources.base() + "/steam"}};
    expect(fetcher.stageOf("steam:777") == Stage::None, "a game not asked for is in no stage");
    fetcher.request(games, {}, {});
    expect(eventually([&] {
               return fetcher.stageOf("steam:777") == Stage::Downloading;
           }),
           "the game being fetched is downloading");
    expect(fetcher.stageOf("steam:440") == Stage::Queued &&
               fetcher.stageOf("steam:404") == Stage::Queued,
           "the games behind it are queued");
    expect(fetcher.pendingGames().size() == 3, "every game queued or downloading is pending");

    fetcher.prioritize({"steam:404"});
    sources.release();
    expect(eventually([&] {
               return fetcher.idle();
           }),
           "the queue drains");
    expect(fetcher.pendingGames().empty() && fetcher.stageOf("steam:404") == Stage::None,
           "a finished or missed game is in no stage");
    std::vector<Fetched> arrived = fetcher.take();
    expect(arrived.size() == 3, "every game settles, saved or not");
    expect(arrived[1].id == "steam:404" && !arrived[1].saved && arrived[1].artwork.empty(),
           "a prioritised game goes next, and its confirmed miss settles it with no art");
    expect(arrived[0].id == "steam:777" && arrived[0].saved && arrived[2].saved,
           "the others arrive with their files");
}

} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "opensu-artwork-test";
    fs::remove_all(root);
    testFetches(root / "fetch");
    testArcadeNames(root / "names");
    testUnreachable(root / "offline");
    testStages(root / "stages");
    fs::remove_all(root);
    std::printf("artwork_fetcher: all checks passed\n");
    return 0;
}
