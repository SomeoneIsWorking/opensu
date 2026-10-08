// The fetcher against a local server standing in for Steam's CDN and libretro-thumbnails.
#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "lucent/http.h"

namespace {

namespace fs = std::filesystem;
using iideck::artwork::ArtworkFetcher;
using iideck::artwork::ArtworkStore;
using iideck::artwork::Fetched;
using iideck::library::Game;
using iideck::library::Source;

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
        all.push_back(std::move(item));
    }
    return all;
}

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
        if (path == "/steam/440/header.jpg") {
            return lucent::http::Response::binary(200, "OK", "image/jpeg", "steam header");
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
    std::vector<std::string> asked_;
    lucent::http::Server server_;
};

void testFetches(const fs::path& root) {
    FakeSources sources;
    ArtworkStore store{root};
    const std::vector<Game> games{
        game(Source::Steam, "steam:440", "440"),
        game(Source::Rom, "rom:sunshine", "gc", "Super Mario Sunshine"),
        game(Source::Rom, "rom:nothing", "gc", "No Such Game (USA)"),
        game(Source::Rom, "rom:switch", "switch", "Xenoblade Chronicles 2"),
    };
    {
        ArtworkFetcher fetcher{store, {sources.base() + "/libretro", sources.base() + "/steam"}};
        fetcher.request(games);
        const std::vector<Fetched> fetched = waitFor(fetcher);
        expect(fetched.size() == 2, "the Steam game and the matched ROM arrive");
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
    expect(!store.wanted(again[0], now) && !store.wanted(again[2], now),
           "neither a kept image nor a recent miss is asked for again");
    expect(store.index("gc", now).has_value(), "the listing is kept for the next run");
}

void testUnreachable(const fs::path& root) {
    ArtworkStore store{root};
    // Port 9 is discard; nothing listens on loopback there.
    ArtworkFetcher fetcher{store, {"http://127.0.0.1:9/libretro", "http://127.0.0.1:9/steam"}};
    const Game steam = game(Source::Steam, "steam:1", "1");
    fetcher.request({steam, game(Source::Steam, "steam:2", "2")});
    expect(waitFor(fetcher).empty(), "nothing arrives from a source that cannot be reached");
    expect(!fs::exists(store.pathFor(steam).string() + ".miss"),
           "an unreachable source is not a miss, so the next run asks again");
}

} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "iideck-artwork-test";
    fs::remove_all(root);
    testFetches(root / "fetch");
    testUnreachable(root / "offline");
    fs::remove_all(root);
    std::printf("artwork_fetcher: all checks passed\n");
    return 0;
}
