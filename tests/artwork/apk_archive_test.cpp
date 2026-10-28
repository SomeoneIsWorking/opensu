// iiSU's APK read by ranges and the UI sounds the fetcher keeps from it, against a local server
// standing in for GitHub's release download.
#include "apk_archive.hpp"

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

#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"
#include "range_server.hpp"
#include "support/zip_fixture.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::artwork::ApkArchive;
using opensu::artwork::ApkAsset;
using opensu::artwork::ApkFile;
using opensu::artwork::ArtworkFetcher;
using opensu::artwork::ArtworkStore;
using opensu::artwork::Fetched;
using opensu::artwork::soundAsset;
using opensu::artwork::fixture::RangeServer;
using opensu::audio::Effect;
using opensu::net::WebClient;
using opensu::test::zipfixture::buildZip;
using opensu::test::zipfixture::FixtureEntry;

constexpr std::string_view navigationWav = "RIFF navigation click";
constexpr std::string_view openWav = "RIFF open";
constexpr std::string_view domeOgg = "OggS domino cue";

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

/// An APK holding three sounds; `fillerEntries` push the directory out of the tail, and `openCrc`
/// is written in place of Open.wav's CRC-32.
std::string apk(int fillerEntries, std::optional<std::uint32_t> openCrc = {}) {
    std::vector<FixtureEntry> entries{
        {"assets/Navigation.wav", std::string{navigationWav}, false, {}},
        {"assets/Open.wav", std::string{openWav}, true, openCrc},
        {"assets/domino_icons_2.ogg", std::string{domeOgg}, false, {}},
        {"classes.dex", std::string(std::size_t{2} * 1024 * 1024, 'd'), false, {}},
    };
    for (int i = 0; i < fillerEntries; ++i) {
        entries.push_back(
            {"res/filler/" + std::string(60, 'a') + std::to_string(i), {}, false, {}});
    }
    return buildZip(entries);
}

void testRanges(int fillerEntries) {
    const std::string served = apk(fillerEntries);
    RangeServer server;
    server.put("/apk", served);
    ApkArchive archive{server.base() + "/apk", served.size()};
    const WebClient web;
    const ApkFile open = archive.fetch(web, "assets/Open.wav");
    expect(open.status == ApkFile::Status::Found && open.bytes == openWav,
           "a deflated entry comes out inflated");
    expect(archive.fetch(web, "assets/Navigation.wav").bytes == navigationWav,
           "a stored entry comes out as stored");
    expect(server.bytesSent() < served.size() / 2, "the APK is not downloaded whole");
    expect(archive.fetch(web, "assets/Close.wav").status == ApkFile::Status::Missing,
           "an entry the APK lacks is missing");
}

void testRefusals() {
    const WebClient web;
    {
        const std::string served = apk(0, 0x0badf00d);
        RangeServer server;
        server.put("/apk", served);
        ApkArchive archive{server.base() + "/apk", served.size()};
        const ApkFile open = archive.fetch(web, "assets/Open.wav");
        expect(open.status == ApkFile::Status::Failed &&
                   open.error.find("CRC") != std::string::npos,
               "an entry that fails its CRC-32 is refused");
    }
    {
        const std::string served = apk(0);
        RangeServer server;
        server.put("/apk", served);
        server.ignoreRange(true);
        ApkArchive archive{server.base() + "/apk", served.size()};
        const ApkFile open = archive.fetch(web, "assets/Open.wav");
        expect(open.status == ApkFile::Status::Failed &&
                   open.error.find("HTTP 200") != std::string::npos,
               "a server that ignores Range is refused");
    }
    ApkArchive offline{"http://127.0.0.1:9/apk", apk(0).size()};
    const ApkFile down = offline.fetch(web, "assets/Open.wav");
    expect(down.status == ApkFile::Status::Failed && !down.error.empty(),
           "an APK that cannot be reached fails rather than misses");
}

void waitIdle(const ArtworkFetcher& fetcher) {
    for (int i = 0; i < 500 && !(fetcher.idle() && i > 0); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
}

void testFetcher(const fs::path& root) {
    const std::string served = apk(0);
    RangeServer server;
    server.put("/apk", served);
    const ArtworkStore store{root};
    {
        ArtworkFetcher fetcher{store,
                               {.libretro = server.base(),
                                .steam = server.base(),
                                .iisuApk = server.base() + "/apk",
                                .iisuApkSize = served.size()}};
        fetcher.request({}, {soundAsset(Effect::Navigation), soundAsset(Effect::Open),
                             soundAsset(Effect::Close), soundAsset(Effect::DominoTwo)});
        waitIdle(fetcher);
        const std::vector<Fetched> fetched = fetcher.take();
        const auto arrived = [&fetched](const std::string& id) {
            return std::ranges::any_of(fetched, [&](const Fetched& one) {
                return one.kind == Fetched::Kind::Sound && one.id == id;
            });
        };
        expect(fetched.size() == 3 && arrived("Navigation.wav") && arrived("Open.wav") &&
                   arrived("domino_icons_2.ogg"),
               "the APK's sounds arrive");
    }
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
    expect(store.assetPath(navigation) == root / "sound" / "Navigation.wav",
           "a sound lands under sound/");
}

void testUnreachable(const fs::path& root) {
    const ArtworkStore store{root};
    ArtworkFetcher fetcher{store,
                           {.libretro = "http://127.0.0.1:9",
                            .steam = "http://127.0.0.1:9",
                            .iisuApk = "http://127.0.0.1:9/apk",
                            .iisuApkSize = apk(0).size()}};
    fetcher.request({}, {soundAsset(Effect::Navigation)});
    waitIdle(fetcher);
    expect(fetcher.take().empty(), "nothing arrives when the APK cannot be reached");
    expect(!fs::exists(store.assetPath(soundAsset(Effect::Navigation)).string() + ".miss"),
           "an unreachable APK is not a miss, so the next run asks again");
}

} // namespace

int main() {
    try {
        const fs::path root = fs::temp_directory_path() / "opensu-apk-archive-test";
        fs::remove_all(root);
        testRanges(0);
        testRanges(1200);
        testRefusals();
        testFetcher(root / "fetch");
        testUnreachable(root / "offline");
        fs::remove_all(root);
        std::printf("apk_archive: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}
