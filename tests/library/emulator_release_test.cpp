// An emulator's official AppImage through the shipping download path, against a fake release host:
// the release JSON read, the download checked against the published SHA-256, a mismatch refused,
// an update kept apart until verified, a release without a digest recorded as seen, and removal.
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include "library/downloaded_emulators.hpp"
#include "library/emulator_release.hpp"
#include "support/release_server.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu::library::roms;
using opensu::test::expect;
using opensu::test::need;

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

const opensu::net::WebClient::Progress anyProgress = [](std::uint64_t, std::uint64_t) {
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
    std::string error;
    const ReleaseAsset first =
        need(release.latest(need(entry.release, "the entry has a release"), error),
             "the release is read");
    const DownloadedEmulator installed = need(
        release.install(entry, first, store, anyProgress, error), "a matching digest installs");
    expect(installed.verified == Verification::Digest && installed.size == 11 &&
               read(installed.file) == "first build",
           "the file is the release's, verified against its digest");
    expect((fs::status(installed.file).permissions() & fs::perms::owner_exec) != fs::perms::none,
           "and executable");
    expect(store.find("emu") == installed, "the record names it as the version in use");

    server.publish("emu", "v1.1", 101, "Emu-1.1-x86_64.AppImage", "tampered", std::string(64, '0'));
    const ReleaseAsset bad =
        need(release.latest(need(entry.release, "the entry has a release"), error),
             "the update is read");
    expect(!release.install(entry, bad, store, anyProgress, error) &&
               error.find("SHA-256") != std::string::npos,
           "a digest mismatch is refused and named");
    expect(store.find("emu") == installed && fs::exists(installed.file),
           "the version before stays in use");
    expect(!fs::exists(store.versionDir("emu", "101") / "Emu-1.1-x86_64.AppImage") &&
               !fs::exists(store.versionDir("emu", "101") / "Emu-1.1-x86_64.AppImage.part"),
           "nothing of the refused download is left");

    server.publish("emu", "v1.2", 102, "Emu-1.2-x86_64.AppImage", "second build");
    const ReleaseAsset good = need(
        release.latest(need(entry.release, "the entry has a release"), error), "the fix is read");
    std::uint64_t seenTotal = 0;
    const DownloadedEmulator updated = need(release.install(
                                                entry, good, store,
                                                [&seenTotal](std::uint64_t, std::uint64_t total) {
                                                    seenTotal = total;
                                                    return true;
                                                },
                                                error),
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
    std::string error;
    const ReleaseAsset asset =
        need(release.latest(need(entry.release, "the entry has a release"), error), "read");
    const DownloadedEmulator installed =
        need(release.install(entry, asset, store, anyProgress, error), "installed");
    expect(installed.verified == Verification::Observed && installed.sha256.size() == 64 &&
               installed.size == 14,
           "without a published digest, the size and SHA-256 seen are recorded");
}

void cancelKeepsNothing(const fs::path& root) {
    opensu::test::ReleaseServer server;
    const EmulatorEntry entry =
        server.entry({.id = "stop", .name = "Stop", .system = "gb", .asset = "^stop\\.AppImage$"});
    const DownloadedEmulators store{root / "emulators"};
    const opensu::net::WebClient web;
    const EmulatorRelease release{web};
    server.publish("stop", "1", 1, "stop.AppImage", std::string(std::size_t{256} * 1024, 'x'));
    std::string error;
    const ReleaseAsset asset =
        need(release.latest(need(entry.release, "the entry has a release"), error), "read");
    expect(!release.install(
               entry, asset, store,
               [](std::uint64_t, std::uint64_t) {
                   return false;
               },
               error) &&
               error == "cancelled",
           "a cancelled download says so");
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
    const ReleaseAsset asset =
        need(release.latest(need(entry.release, "the entry has a release"), error), "read");
    expect(release.install(entry, asset, store, anyProgress, error).has_value(), "installed");
    const fs::path neighbour = root / "emulators" / "other" / "keep";
    fs::create_directories(neighbour.parent_path());
    std::ofstream{neighbour} << "x";
    expect(store.remove("gone", error) && !store.find("gone") &&
               !fs::exists(root / "emulators" / "gone"),
           "remove deletes every version of that emulator");
    expect(fs::exists(neighbour), "and nothing else");
}

} // namespace

int main() {
    try {
        const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "emulator_release";
        std::error_code ec;
        fs::remove_all(root, ec);
        assetsAreRead();
        digestMatchesAndMismatches(root / "digest");
        noDigestIsObserved(root / "observed");
        cancelKeepsNothing(root / "cancel");
        removeDeletesOnlyOurs(root / "remove");
        fs::remove_all(root, ec);
        std::puts("emulator_release: ok");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}
