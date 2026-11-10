// An emulator's official AppImage through the shipping download path, against a fake release host:
// the release JSON read, the download checked against the published SHA-256, a mismatch refused,
// an update kept apart until verified, a release without a digest recorded as seen, an AppImage
// taken out of the zip it ships in, and removal.
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stop_token>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "library/downloaded_emulators.hpp"
#include "library/emulator_release.hpp"
#include "support/release_server.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"
#include "zip/zip_writer.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu::library::roms;
using opensu::test::expect;
using opensu::test::need;

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

constexpr auto anyProgress = [](std::uint64_t, std::uint64_t) {
    return true;
};

void assetsAreRead() {
    const ReleaseSource source{"https://api.example/r", "^Emu-[0-9.]+-x86_64\\.AppImage$",
                               "https://github.com/emu/emu/releases/download/"};
    std::string error;
    const ReleaseAsset asset = need(releaseAssetOf(R"({"tag_name":"v1.2","assets":[
            {"id":7,"name":"Emu-1.2-aarch64.AppImage","browser_download_url":"https://github.com/emu/emu/releases/download/v1.2/Emu-1.2-aarch64.AppImage"},
            {"id":8,"name":"Emu-1.2-x86_64.AppImage","size":10,"digest":"sha256:ABCD",
             "browser_download_url":"https://github.com/emu/emu/releases/download/v1.2/Emu-1.2-x86_64.AppImage"}]})",
                                                   source, error),
                                    "the matching asset of a GitHub release is found");
    expect(asset.version == "v1.2" && asset.assetId == "8" && asset.size == 10 &&
               asset.sha256 == "abcd",
           "its tag, id, size and digest are read");

    const ReleaseAsset rolling =
        need(releaseAssetOf(R"({"tag_name":"latest","published_at":"2026-09-30T10:00:00Z","assets":[
            {"id":"9","name":"Emu-2-x86_64.AppImage","digest":null,"browser_download_url":"https://github.com/emu/emu/releases/download/latest/Emu-2-x86_64.AppImage"}]})",
                            source, error),
             "a release whose digest is null, as GitHub sends it, is read");
    expect(rolling.version == "latest 2026-09-30" && rolling.sha256.empty(),
           "a rolling tag is dated, and no digest is no digest");

    expect(!releaseAssetOf(R"({"tag_name":"v1","assets":[{"id":1,"name":"Emu-1-x86_64.AppImage",
            "browser_download_url":"https://mirror.example/Emu-1-x86_64.AppImage"}]})",
                           source, error) &&
               error.find("own host") != std::string::npos,
           "an asset off the project's own host is refused");
    expect(!releaseAssetOf(R"({"tag_name":"v1","assets":[]})", source, error) &&
               error.find("no file matching") != std::string::npos,
           "a release without the AppImage says so");
}

void digestMatchesAndMismatches(const fs::path& root) {
    opensu::test::ReleaseServer server;
    const EmulatorEntry entry = server.entry(
        {.id = "emu", .name = "Emu", .system = "gba", .asset = "^Emu-[0-9.]+-x86_64\\.AppImage$"});
    const DownloadedEmulators store{root / "emulators"};
    const opensu::net::WebClient web;
    const EmulatorRelease release{web};

    server.publish("emu", "v1.0", 100, "Emu-1.0-x86_64.AppImage", "first build");
    const ReleaseAsset first =
        need(release.latest(std::stop_token{}, need(entry.release, "the entry has a release")),
             "the release is read");
    const DownloadedEmulator installed =
        need(release.install(std::stop_token{}, entry, first, store, anyProgress),
             "a matching digest installs");
    expect(installed.verified == Verification::Digest && installed.size == 11 &&
               read(installed.file) == "first build",
           "the file is the release's, verified against its digest");
    expect((fs::status(installed.file).permissions() & fs::perms::owner_exec) != fs::perms::none,
           "and executable");
    expect(store.find("emu") == installed, "the record names it as the version in use");

    server.publish("emu", "v1.1", 101, "Emu-1.1-x86_64.AppImage", "tampered", std::string(64, '0'));
    const ReleaseAsset bad =
        need(release.latest(std::stop_token{}, need(entry.release, "the entry has a release")),
             "the update is read");
    expect(release.install(std::stop_token{}, entry, bad, store, anyProgress)
                   .error()
                   .find("SHA-256") != std::string::npos,
           "a digest mismatch is refused and named");
    expect(store.find("emu") == installed && fs::exists(installed.file),
           "the version before stays in use");
    expect(!fs::exists(store.versionDir("emu", "101") / "Emu-1.1-x86_64.AppImage") &&
               !fs::exists(store.versionDir("emu", "101") / "Emu-1.1-x86_64.AppImage.part"),
           "nothing of the refused download is left");

    server.publish("emu", "v1.2", 102, "Emu-1.2-x86_64.AppImage", "second build");
    const ReleaseAsset good =
        need(release.latest(std::stop_token{}, need(entry.release, "the entry has a release")),
             "the fix is read");
    std::uint64_t seenTotal = 0;
    const DownloadedEmulator updated =
        need(release.install(std::stop_token{}, entry, good, store,
                             [&seenTotal](std::uint64_t, std::uint64_t total) {
                                 seenTotal = total;
                                 return true;
                             }),
             "a good update installs");
    expect(seenTotal == 12, "progress names the size");
    expect(store.find("emu") == updated && updated.version == "v1.2",
           "the update is the version in use once verified");
    expect(!fs::exists(installed.file), "and the old version goes after the switch");
}

void noDigestIsObserved(const fs::path& root) {
    opensu::test::ReleaseServer server;
    const EmulatorEntry entry = server.entry(
        {.id = "plain", .name = "Plain", .system = "gb", .asset = "^plain\\.appimage$"});
    const DownloadedEmulators store{root / "emulators"};
    const opensu::net::WebClient web;
    const EmulatorRelease release{web};
    server.publish("plain", "0.10.5", 5, "plain.appimage", "no digest here", "");
    const ReleaseAsset asset = need(
        release.latest(std::stop_token{}, need(entry.release, "the entry has a release")), "read");
    const DownloadedEmulator installed =
        need(release.install(std::stop_token{}, entry, asset, store, anyProgress), "installed");
    expect(installed.verified == Verification::Observed && installed.sha256.size() == 64 &&
               installed.size == 14,
           "without a published digest, the size and SHA-256 seen are recorded");
}

std::string zipOf(const std::vector<std::pair<std::string, std::string>>& files) {
    std::ostringstream out;
    opensu::zip::Writer writer{out};
    std::string error;
    for (const auto& [name, bytes] : files) {
        expect(writer.add(name, bytes, error), "the test zip takes each file");
    }
    expect(writer.finish(error), "the test zip is written");
    return out.str();
}

void zipMemberIsUnpacked(const fs::path& root) {
    opensu::test::ReleaseServer server;
    const EmulatorEntry entry = server.entry({.id = "zipped",
                                              .name = "Zipped",
                                              .system = "nds",
                                              .asset = "^Zipped-[0-9.]+-x86_64\\.zip$",
                                              .member = "^Zipped-x86_64\\.AppImage$"});
    const DownloadedEmulators store{root / "emulators"};
    const opensu::net::WebClient web;
    const EmulatorRelease release{web};
    const std::string archive =
        zipOf({{"README.txt", "read me"}, {"Zipped-x86_64.AppImage", "the appimage"}});
    server.publish("zipped", "1.1", 21, "Zipped-1.1-x86_64.zip", archive);
    const ReleaseAsset asset = need(
        release.latest(std::stop_token{}, need(entry.release, "the entry has a release")), "read");
    const DownloadedEmulator installed =
        need(release.install(std::stop_token{}, entry, asset, store, anyProgress),
             "a zipped AppImage installs");
    expect(installed.file.filename() == "Zipped-x86_64.AppImage" &&
               read(installed.file) == "the appimage",
           "the AppImage is taken out of the zip");
    expect((fs::status(installed.file).permissions() & fs::perms::owner_exec) != fs::perms::none,
           "and is executable");
    expect(installed.verified == Verification::Digest && installed.size == archive.size(),
           "the record is the verified download's");
    expect(!fs::exists(store.versionDir("zipped", "21") / "Zipped-1.1-x86_64.zip.part"),
           "the zip is not kept");
    expect(store.find("zipped") == installed, "the record names it as the version in use");

    server.publish("zipped", "1.2", 22, "Zipped-1.2-x86_64.zip", zipOf({{"other", "x"}}));
    const ReleaseAsset empty = need(
        release.latest(std::stop_token{}, need(entry.release, "the entry has a release")), "read");
    expect(release.install(std::stop_token{}, entry, empty, store, anyProgress)
                   .error()
                   .find("no file matching") != std::string::npos,
           "a zip without the AppImage is refused");
    expect(store.find("zipped") == installed && fs::is_empty(store.versionDir("zipped", "22")),
           "the version before stays and nothing of the refused one is left");
}

void cancelKeepsNothing(const fs::path& root) {
    opensu::test::ReleaseServer server;
    const EmulatorEntry entry =
        server.entry({.id = "stop", .name = "Stop", .system = "gb", .asset = "^stop\\.AppImage$"});
    const DownloadedEmulators store{root / "emulators"};
    const opensu::net::WebClient web;
    const EmulatorRelease release{web};
    server.publish("stop", "1", 1, "stop.AppImage", std::string(std::size_t{256} * 1024, 'x'));
    const ReleaseAsset asset = need(
        release.latest(std::stop_token{}, need(entry.release, "the entry has a release")), "read");
    expect(release
               .install(std::stop_token{}, entry, asset, store,
                        [](std::uint64_t, std::uint64_t) {
                            return false;
                        })
               .stopped(),
           "a cancelled download is stopped");
    expect(!store.find("stop") && !fs::exists(store.versionDir("stop", "1") / "stop.AppImage.part"),
           "and leaves nothing installed or partial");
}

void removeDeletesOnlyOurs(const fs::path& root) {
    opensu::test::ReleaseServer server;
    const EmulatorEntry entry =
        server.entry({.id = "gone", .name = "Gone", .system = "gb", .asset = "^gone\\.AppImage$"});
    const DownloadedEmulators store{root / "emulators"};
    const opensu::net::WebClient web;
    const EmulatorRelease release{web};
    server.publish("gone", "1", 3, "gone.AppImage", "bytes");
    std::string error;
    const ReleaseAsset asset = need(
        release.latest(std::stop_token{}, need(entry.release, "the entry has a release")), "read");
    expect(release.install(std::stop_token{}, entry, asset, store, anyProgress).ok(), "installed");
    const fs::path neighbour = root / "emulators" / "other" / "keep";
    fs::create_directories(neighbour.parent_path());
    std::ofstream{neighbour} << "x";
    expect(store.remove("gone", error) && !store.find("gone") &&
               !fs::exists(root / "emulators" / "gone"),
           "remove deletes every version of that emulator");
    expect(fs::exists(neighbour), "and nothing else");
}

} // namespace

int main() try {
    try {
        const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "emulator_release";
        std::error_code ec;
        fs::remove_all(root, ec);
        assetsAreRead();
        digestMatchesAndMismatches(root / "digest");
        noDigestIsObserved(root / "observed");
        zipMemberIsUnpacked(root / "zip");
        cancelKeepsNothing(root / "cancel");
        removeDeletesOnlyOurs(root / "remove");
        fs::remove_all(root, ec);
        std::puts("emulator_release: ok");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
} catch (...) {
    return opensu::test::escaped();
}
