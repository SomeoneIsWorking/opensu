// iiSU's console cards and tab glyphs, fetched for the player who chose iiSU's console art,
// against a local server standing in for GitHub's release download.
#include "starter_pack.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <webp/encode.h>

#include "apk_archive.hpp"
#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"
#include "console_glyphs.hpp"
#include "range_server.hpp"
#include "support/zip_fixture.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::artwork::ApkArchive;
using opensu::artwork::ApkFile;
using opensu::artwork::ArtworkFetcher;
using opensu::artwork::ArtworkStore;
using opensu::artwork::ConsoleGlyphs;
using opensu::artwork::Fetched;
using opensu::artwork::PackPin;
using opensu::artwork::StarterPack;
using opensu::artwork::fixture::RangeServer;
using opensu::net::WebClient;
using opensu::test::zipfixture::buildZip;
using opensu::test::zipfixture::crcOf;
using opensu::test::zipfixture::FixtureEntry;

constexpr int cardSize = 8;
constexpr std::string_view packEntry = "assets/iiSU_StarterPack.zip";
constexpr std::string_view packFile = "test-pack.zip";
// The border pack maps gc to a logo in the APK and snes to one the APK lacks; ps4 is not in it.
constexpr std::string_view borderPack =
    R"({"version": 9, "consoles": [)"
    R"({"console": "GC", "border": "gc.png", "logo": "logo_gc.png"},)"
    R"({"console": "snes", "border": "SNES.png", "logo": "logo_SNES.png"}]})";
constexpr std::string_view gcGlyph = "\x89PNG\r\n\x1a\n glyph of the GameCube";

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

/// What the test serves as the APK, and the pin that describes its pack.
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
    entries.push_back({"assets/borders/border_pack.json", std::string{borderPack}, true, {}});
    entries.push_back({"assets/borders/logo_gc.png", std::string{gcGlyph}, false, {}});
    entries.push_back({"classes.dex", std::string(std::size_t{2} * 1024 * 1024, 'd'), false, {}});
    for (int i = 0; i < fillerEntries; ++i) {
        entries.push_back(
            {"res/filler/" + std::string(60, 'a') + std::to_string(i), {}, false, {}});
    }
    out.apk = buildZip(entries);
    out.pin = PackPin{packEntry, static_cast<std::uint32_t>(out.pack.size()),
                      packCrc.value_or(crcOf(out.pack)), packFile};
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

void waitIdle(const ArtworkFetcher& fetcher) {
    for (int i = 0; i < 500 && !(fetcher.idle() && i > 0); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
}

void testDownload(const fs::path& root, int fillerEntries) {
    const Release served = release(fillerEntries);
    RangeServer server;
    server.put("/apk", served.apk);
    const ArtworkStore store{root};
    ApkArchive apk{server.base() + "/apk", served.apk.size()};
    const StarterPack pack{store, apk, served.pin};
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

void testGlyphs() {
    const Release served = release(0);
    RangeServer server;
    server.put("/apk", served.apk);
    ApkArchive apk{server.base() + "/apk", served.apk.size()};
    ConsoleGlyphs glyphs{apk};
    const WebClient web;
    const ApkFile gc = glyphs.fetch(web, "gc");
    expect(gc.status == ApkFile::Status::Found && gc.bytes == gcGlyph,
           "a console's glyph comes out of the APK as stored, found by its folded name");
    expect(server.bytesSent() < served.apk.size() / 2, "the APK is not downloaded whole");
    const std::uint64_t asked = server.requests();
    expect(glyphs.fetch(web, "ps4").status == ApkFile::Status::Missing,
           "a system the border pack lacks has no glyph");
    expect(server.requests() == asked, "the border pack is read once");
    expect(glyphs.fetch(web, "snes").status == ApkFile::Status::Missing,
           "a logo the APK lacks is no glyph");
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
        ApkArchive apk{server.base() + "/apk", served.apk.size()};
        expect(!StarterPack{store, apk, wrongSize}.ensure(web, error),
               "a pack of another size than pinned is refused");
        expect(!fs::exists(store.packPath(wrongSize.fileName)), "nothing is stored for it");
    }
    {
        const Release served = release(0, 0x0badf00d);
        RangeServer server;
        server.put("/apk", served.apk);
        const ArtworkStore store{root / "crc"};
        ApkArchive apk{server.base() + "/apk", served.apk.size()};
        expect(!StarterPack{store, apk, served.pin}.ensure(web, error) &&
                   error.find("CRC") != std::string::npos,
               "a pack whose bytes do not match its CRC-32 is refused");
        expect(!fs::exists(store.packPath(served.pin.fileName)), "nothing is stored for it");
    }
}

void testFetcher(const fs::path& root) {
    const Release served = release(0);
    RangeServer server;
    server.put("/apk", served.apk);
    const ArtworkStore store{root};
    {
        ArtworkFetcher fetcher{store,
                               {.libretro = server.base(),
                                .steam = server.base(),
                                .iisuApk = server.base() + "/apk",
                                .iisuApkSize = served.apk.size(),
                                .iisuPin = served.pin}};
        fetcher.request({}, {}, {"gc", "ps4", "snes"});
        waitIdle(fetcher);
        const std::vector<Fetched> fetched = fetcher.take();
        const auto arrived = [&fetched](Fetched::Kind kind, const std::string& id) {
            return std::ranges::any_of(fetched, [&](const Fetched& one) {
                return one.saved && one.kind == kind && one.id == id;
            });
        };
        expect(arrived(Fetched::Kind::Card, "gc") && arrived(Fetched::Kind::Glyph, "gc") &&
                   !arrived(Fetched::Kind::Card, "ps4") && !arrived(Fetched::Kind::Glyph, "snes"),
               "the GameCube's card and glyph arrive, and nothing for systems iiSU lacks");
        expect(fetcher.consoleArt() == opensu::artwork::ConsoleArtProgress{},
               "the console art round ends with nothing pending and no error");
    }
    expect(isPngOfCardSize(read(store.cardPath("gc"))), "the card is in the store as a PNG");
    expect(read(store.glyphPath("gc")) == gcGlyph, "the glyph is in the store as shipped");
    expect(fs::exists(store.cardPath("ps4").string() + ".miss") &&
               fs::exists(store.glyphPath("ps4").string() + ".miss") &&
               fs::exists(store.glyphPath("snes").string() + ".miss"),
           "a system without a card or glyph is a miss");
    const auto now = ArtworkStore::Clock::now();
    expect(!store.wantedCard("gc", now) && !store.wantedCard("ps4", now) &&
               !store.wantedGlyph("gc", now) && !store.wantedGlyph("ps4", now),
           "neither kept art nor a recent miss is asked for again");
    expect(store.storedCard("gc") == store.cardPath("gc") && store.storedCard("ps4").empty() &&
               store.storedGlyph("gc") == store.glyphPath("gc"),
           "stored art is found on the next start");
}

void testUnreachable(const fs::path& root) {
    const ArtworkStore store{root};
    ArtworkFetcher fetcher{store,
                           {.libretro = "http://127.0.0.1:9",
                            .steam = "http://127.0.0.1:9",
                            .iisuApk = "http://127.0.0.1:9/apk",
                            .iisuApkSize = release(0).apk.size(),
                            .iisuPin = release(0).pin}};
    fetcher.request({}, {}, {"gc"});
    waitIdle(fetcher);
    expect(std::ranges::none_of(fetcher.take(), &Fetched::saved),
           "nothing arrives when the APK cannot be reached");
    expect(!fetcher.consoleArt().error.empty(), "the settings row can say why");
    expect(!fs::exists(store.cardPath("gc").string() + ".miss") &&
               !fs::exists(store.glyphPath("gc").string() + ".miss"),
           "an unreachable APK is not a miss, so the next request asks again");
}

} // namespace

int main() {
    try {
        const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "starter-pack";
        fs::remove_all(root);
        testDownload(root / "small", 0);
        testDownload(root / "large", 1200);
        testGlyphs();
        testRefusals(root / "refuse");
        testFetcher(root / "fetch");
        testUnreachable(root / "offline");
        fs::remove_all(root);
        std::printf("starter_pack: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}
