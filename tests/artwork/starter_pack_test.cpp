// The starter pack and the console cards it feeds, against a local server standing in for
// GitHub's release download.
#include "starter_pack.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <webp/encode.h>

#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"
#include "range_server.hpp"
#include "zip_fixture.hpp"

namespace {

namespace fs = std::filesystem;
using iideck::artwork::ArtworkFetcher;
using iideck::artwork::ArtworkStore;
using iideck::artwork::Fetched;
using iideck::artwork::PackPin;
using iideck::artwork::StarterPack;
using iideck::artwork::fixture::buildZip;
using iideck::artwork::fixture::crcOf;
using iideck::artwork::fixture::FixtureEntry;
using iideck::artwork::fixture::RangeServer;
using iideck::library::Console;
using iideck::library::ShelfItem;
using iideck::net::WebClient;

constexpr int cardSize = 8;
constexpr std::string_view packEntry = "assets/iiSU_StarterPack.zip";

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

/// A `cardSize` square lossless WebP.
std::string webpCard() {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(cardSize * cardSize * 4), 0x80);
    std::uint8_t* encoded = nullptr;
    const std::size_t size =
        WebPEncodeLosslessRGBA(pixels.data(), cardSize, cardSize, cardSize * 4, &encoded);
    expect(size > 0, "libwebp encodes the fixture card");
    std::string out{reinterpret_cast<const char*>(encoded), size};
    WebPFree(encoded);
    return out;
}

/// What the test serves as the APK, and the pin that describes it.
struct Release {
    std::string apk;
    std::string pack;
    PackPin pin;
};

/// An APK whose pack holds a gc card; `fillerEntries` of them push the directory out of the tail.
Release release(int fillerEntries, std::optional<std::uint32_t> packCrc = {}) {
    Release out;
    out.pack = buildZip({{"platforms/gc.webp", webpCard(), true, {}},
                         {"platforms/gc_title.webp", "title", false, {}}});
    std::vector<FixtureEntry> entries{{std::string{packEntry}, out.pack, true, packCrc}};
    entries.push_back({"classes.dex", std::string(2 * 1024 * 1024, 'd'), false, {}});
    for (int i = 0; i < fillerEntries; ++i) {
        entries.push_back(
            {"res/filler/" + std::string(60, 'a') + std::to_string(i), {}, false, {}});
    }
    out.apk = buildZip(entries);
    out.pin = PackPin{out.apk.size(), packEntry, static_cast<std::uint32_t>(out.pack.size()),
                      packCrc.value_or(crcOf(out.pack)), "test-pack.zip"};
    return out;
}

bool isPngOfCardSize(const std::string& png) {
    const auto be32 = [&png](std::size_t at) {
        return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(png[at])) << 24) |
               (static_cast<std::uint32_t>(static_cast<std::uint8_t>(png[at + 1])) << 16) |
               (static_cast<std::uint32_t>(static_cast<std::uint8_t>(png[at + 2])) << 8) |
               static_cast<std::uint32_t>(static_cast<std::uint8_t>(png[at + 3]));
    };
    return png.size() > 24 && png.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0 && be32(16) == cardSize &&
           be32(20) == cardSize;
}

void testDownload(const fs::path& root, int fillerEntries) {
    const Release served = release(fillerEntries);
    RangeServer server;
    server.put("/apk", served.apk);
    const ArtworkStore store{root};
    const StarterPack pack{store, server.base() + "/apk", served.pin};
    const WebClient web;
    std::string error;
    expect(pack.ensure(web, error), "the pack downloads");
    expect(read(store.packPath(served.pin.fileName)) == served.pack,
           "the stored pack is the APK's entry, inflated");
    expect(server.bytesSent() < served.apk.size() / 2, "the APK is not downloaded whole");
    const std::uint64_t asked = server.requests();
    expect(pack.ensure(web, error) && server.requests() == asked,
           "a stored pack is not downloaded again");

    const std::optional<std::string> card = pack.card("gc", error);
    expect(card.has_value() && isPngOfCardSize(*card), "a card comes out as a PNG of its size");
    expect(!pack.card("ps4", error) && error.empty(), "a system without a card is nothing");
}

void testRefusals(const fs::path& root) {
    std::string error;
    const WebClient web;
    {
        const Release served = release(0);
        RangeServer server;
        server.put("/apk", served.apk);
        PackPin wrongSize = served.pin;
        ++wrongSize.entrySize;
        const ArtworkStore store{root / "size"};
        expect(!StarterPack{store, server.base() + "/apk", wrongSize}.ensure(web, error),
               "an entry of another size than pinned is refused");
        expect(!fs::exists(store.packPath(wrongSize.fileName)), "nothing is stored for it");
    }
    {
        const Release served = release(0, 0x0badf00d);
        RangeServer server;
        server.put("/apk", served.apk);
        const ArtworkStore store{root / "crc"};
        expect(!StarterPack{store, server.base() + "/apk", served.pin}.ensure(web, error) &&
                   error.find("CRC") != std::string::npos,
               "an entry that fails its CRC-32 is refused");
        expect(!fs::exists(store.packPath(served.pin.fileName)), "nothing is stored for it");
    }
    {
        const Release served = release(0);
        RangeServer server;
        server.put("/apk", served.apk);
        server.ignoreRange(true);
        const ArtworkStore store{root / "norange"};
        expect(!StarterPack{store, server.base() + "/apk", served.pin}.ensure(web, error) &&
                   error.find("HTTP 200") != std::string::npos,
               "a server that ignores Range is refused");
    }
}

void testFetcher(const fs::path& root) {
    const Release served = release(0);
    RangeServer server;
    server.put("/apk", served.apk);
    const ArtworkStore store{root};
    const Console gc{"gc", "GameCube", 1, {}};
    const Console ps4{"ps4", "PlayStation 4", 1, {}};
    {
        ArtworkFetcher fetcher{store,
                               {server.base(), server.base(), server.base() + "/apk", served.pin}};
        fetcher.request({}, {gc, ps4});
        for (int i = 0; i < 500 && !(fetcher.idle() && i > 0); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        const std::vector<Fetched> fetched = fetcher.take();
        expect(fetched.size() == 1 && fetched[0].kind == Fetched::Kind::Console &&
                   fetched[0].id == "gc" && fetched[0].artwork == store.pathFor(gc),
               "the GameCube card arrives as a console's artwork");
    }
    expect(isPngOfCardSize(read(store.pathFor(gc))), "the card is in the store as a PNG");
    expect(fs::exists(store.pathFor(ps4).string() + ".miss"), "a system without a card is a miss");
    const auto now = ArtworkStore::Clock::now();
    expect(!store.wanted(gc, now) && !store.wanted(ps4, now),
           "neither a kept card nor a recent miss is asked for again");

    std::vector<ShelfItem> shelf{gc, ps4};
    store.apply(shelf);
    expect(std::get<Console>(shelf[0]).artwork == store.pathFor(gc) &&
               std::get<Console>(shelf[1]).artwork.empty(),
           "the next start reads a stored card from the store");
}

void testUnreachable(const fs::path& root) {
    const ArtworkStore store{root};
    const Console gc{"gc", "GameCube", 1, {}};
    ArtworkFetcher fetcher{
        store,
        {"http://127.0.0.1:9", "http://127.0.0.1:9", "http://127.0.0.1:9/apk", release(0).pin}};
    fetcher.request({}, {gc});
    for (int i = 0; i < 500 && !(fetcher.idle() && i > 0); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    expect(fetcher.take().empty(), "nothing arrives when the APK cannot be reached");
    expect(!fs::exists(store.pathFor(gc).string() + ".miss"),
           "an unreachable APK is not a miss, so the next run asks again");
}

} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "iideck-starter-pack-test";
    fs::remove_all(root);
    testDownload(root / "small", 0);
    testDownload(root / "large", 1200);
    testRefusals(root / "refuse");
    testFetcher(root / "fetch");
    testUnreachable(root / "offline");
    fs::remove_all(root);
    std::printf("starter_pack: all checks passed\n");
    return 0;
}
