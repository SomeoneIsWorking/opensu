// The starter pack and the console cards it feeds, against a local server standing in for
// GitHub's release download.
#include "starter_pack.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <webp/encode.h>

#include "apk_archive.hpp"
#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"
#include "console_glyphs.hpp"
#include "range_server.hpp"
#include "zip_fixture.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::artwork::ApkArchive;
using opensu::artwork::ApkAsset;
using opensu::artwork::ApkFile;
using opensu::artwork::ArtworkFetcher;
using opensu::artwork::ArtworkStore;
using opensu::artwork::ConsoleGlyphs;
using opensu::artwork::Fetched;
using opensu::artwork::navAsset;
using opensu::artwork::PackPin;
using opensu::artwork::soundAsset;
using opensu::artwork::StarterPack;
using opensu::artwork::fixture::buildZip;
using opensu::artwork::fixture::crcOf;
using opensu::artwork::fixture::FixtureEntry;
using opensu::artwork::fixture::RangeServer;
using opensu::audio::Effect;
using opensu::library::Console;
using opensu::library::Section;
using opensu::library::ShelfItem;
using opensu::net::WebClient;

constexpr int cardSize = 8;
constexpr std::string_view packEntry = "assets/iiSU_StarterPack.zip";
// The border pack maps gc to a logo in the APK and snes to one the APK lacks; ps4 is not in it.
constexpr std::string_view borderPack =
    R"({"version": 9, "consoles": [)"
    R"({"console": "GC", "border": "gc.png", "logo": "logo_gc.png"},)"
    R"({"console": "snes", "border": "SNES.png", "logo": "logo_SNES.png"}]})";
constexpr std::string_view navigationWav = "RIFF navigation click";
constexpr std::string_view openWav = "RIFF open";
constexpr std::string_view domeOgg = "OggS domino cue";
constexpr std::string_view homePng = "\x89PNG\r\n\x1a\n the Home icon";
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
    entries.push_back({"assets/borders/border_pack.json", std::string{borderPack}, true, {}});
    entries.push_back({"assets/borders/logo_gc.png", std::string{gcGlyph}, false, {}});
    entries.push_back({"assets/Navigation.wav", std::string{navigationWav}, false, {}});
    entries.push_back({"assets/Open.wav", std::string{openWav}, true, {}});
    entries.push_back({"assets/domino_icons_2.ogg", std::string{domeOgg}, false, {}});
    entries.push_back({"res/TG.png", std::string{homePng}, false, {}});
    entries.push_back({"classes.dex", std::string(std::size_t{2} * 1024 * 1024, 'd'), false, {}});
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
    ApkArchive apk{server.base() + "/apk", served.pin.apkSize};
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
    ApkArchive apk{server.base() + "/apk", served.pin.apkSize};
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

    ApkArchive offline{"http://127.0.0.1:9/apk", served.pin.apkSize};
    ConsoleGlyphs unreachable{offline};
    const ApkFile down = unreachable.fetch(web, "gc");
    expect(down.status == ApkFile::Status::Failed && !down.error.empty(),
           "an APK that cannot be reached fails rather than misses");
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
        ApkArchive apk{server.base() + "/apk", wrongSize.apkSize};
        expect(!StarterPack{store, apk, wrongSize}.ensure(web, error),
               "an entry of another size than pinned is refused");
        expect(!fs::exists(store.packPath(wrongSize.fileName)), "nothing is stored for it");
    }
    {
        const Release served = release(0, 0x0badf00d);
        RangeServer server;
        server.put("/apk", served.apk);
        const ArtworkStore store{root / "crc"};
        ApkArchive apk{server.base() + "/apk", served.pin.apkSize};
        expect(!StarterPack{store, apk, served.pin}.ensure(web, error) &&
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
        ApkArchive apk{server.base() + "/apk", served.pin.apkSize};
        expect(!StarterPack{store, apk, served.pin}.ensure(web, error) &&
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
    const Console snes{"snes", "Super Nintendo", 1, {}};
    {
        ArtworkFetcher fetcher{store,
                               {server.base(), server.base(), server.base() + "/apk", served.pin}};
        fetcher.request({}, {gc, ps4, snes},
                        {soundAsset(Effect::Navigation), soundAsset(Effect::Open),
                         soundAsset(Effect::Close), soundAsset(Effect::DominoTwo),
                         navAsset({Section::Home, false}), navAsset({Section::Home, true})});
        for (int i = 0; i < 500 && !(fetcher.idle() && i > 0); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        const std::vector<Fetched> fetched = fetcher.take();
        const auto arrived = [&fetched](Fetched::Kind kind, const std::string& id) {
            return std::ranges::any_of(fetched, [&](const Fetched& one) {
                return one.kind == kind && one.id == id;
            });
        };
        expect(fetched.size() == 6 && arrived(Fetched::Kind::Console, "gc") &&
                   arrived(Fetched::Kind::Glyph, "gc") &&
                   arrived(Fetched::Kind::Sound, "Navigation.wav") &&
                   arrived(Fetched::Kind::Sound, "Open.wav") &&
                   arrived(Fetched::Kind::Sound, "domino_icons_2.ogg") &&
                   arrived(Fetched::Kind::NavIcon, "home.png"),
               "the GameCube card and glyph, the APK's sounds and the Home icon arrive");
    }
    expect(read(store.glyphPath("gc")) == gcGlyph, "the glyph is in the store as shipped");
    expect(fs::exists(store.glyphPath("ps4").string() + ".miss") &&
               fs::exists(store.glyphPath("snes").string() + ".miss"),
           "a system without a glyph is a miss");
    expect(!store.wantedGlyph("gc", ArtworkStore::Clock::now()) &&
               !store.wantedGlyph("ps4", ArtworkStore::Clock::now()),
           "neither a kept glyph nor a recent miss is asked for again");
    expect(store.storedGlyph("gc") == store.glyphPath("gc") && store.storedGlyph("ps4").empty(),
           "a stored glyph is found on the next start");
    const ApkAsset navigation = soundAsset(Effect::Navigation);
    const ApkAsset open = soundAsset(Effect::Open);
    const ApkAsset close = soundAsset(Effect::Close);
    expect(read(store.assetPath(navigation)) == navigationWav &&
               read(store.assetPath(open)) == openWav &&
               read(store.assetPath(soundAsset(Effect::DominoTwo))) == domeOgg,
           "a sound lands in the store as the APK holds it, stored or deflated");
    expect(fs::exists(store.assetPath(close).string() + ".miss") &&
               store.storedAsset(close).empty() && store.storedAsset(open) == store.assetPath(open),
           "a sound the APK lacks is a miss; a kept one is found on the next start");
    expect(!store.wantedAsset(navigation, ArtworkStore::Clock::now()) &&
               !store.wantedAsset(close, ArtworkStore::Clock::now()),
           "neither a kept sound nor a recent miss is asked for again");
    const ApkAsset home = navAsset({Section::Home, false});
    const ApkAsset homeSelected = navAsset({Section::Home, true});
    expect(read(store.assetPath(home)) == homePng &&
               store.assetPath(home) == root / "nav" / "home.png",
           "a dock icon lands under nav/ as the APK holds it");
    expect(fs::exists(store.assetPath(homeSelected).string() + ".miss") &&
               !store.wantedAsset(homeSelected, ArtworkStore::Clock::now()),
           "an icon the APK lacks is a miss");
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
    fetcher.request({}, {gc}, {soundAsset(Effect::Navigation), navAsset({Section::Home, false})});
    for (int i = 0; i < 500 && !(fetcher.idle() && i > 0); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    expect(fetcher.take().empty(), "nothing arrives when the APK cannot be reached");
    expect(!fs::exists(store.pathFor(gc).string() + ".miss") &&
               !fs::exists(store.assetPath(soundAsset(Effect::Navigation)).string() + ".miss") &&
               !fs::exists(store.assetPath(navAsset({Section::Home, false})).string() + ".miss"),
           "an unreachable APK is not a miss, so the next run asks again");
}

} // namespace

int main() {
    try {
        const fs::path root = fs::temp_directory_path() / "opensu-starter-pack-test";
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
