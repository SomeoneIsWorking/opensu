// A plugin's manifest: the fields it must have, the read paths' tokens expanded on this machine,
// and the permission list that a player's consent is compared against.
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "plugin/manifest.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu::plugin;
using opensu::test::expect;

void readsAWholeManifest() {
    const ManifestRead read = parseManifest(R"({
        "id": "eden-playtime", "name": "Eden play time", "version": "1.0.0", "api": 1,
        "permissions": {"read": {"b": ["{xdg_data}/eden"], "a": ["{home}/x", "{xdg_data}/eden"]},
                        "http": ["api.example.com"]}})");
    expect(read.error.empty(), "a whole manifest is read");
    const Manifest& manifest = opensu::test::need(read.manifest, "a whole manifest is read");
    expect(manifest.id == "eden-playtime" && manifest.api == 1 && manifest.read.size() == 2,
           "its fields");
    expect(manifest.permissions() == std::vector<std::string>{"http:api.example.com",
                                                              "read:{home}/x",
                                                              "read:{xdg_data}/eden"},
           "the permissions are one sorted list without repeats");
}

void refusesWhatItCannotTrust() {
    const auto refused = [](const char* text) {
        return !parseManifest(text).manifest.has_value();
    };
    expect(refused("not json"), "not JSON");
    expect(refused(R"({"id": "Eden", "name": "n", "version": "1", "api": 1})"),
           "an id with capitals");
    expect(refused(R"({"id": "e", "version": "1", "api": 1})"), "no name");
    expect(refused(R"({"id": "e", "name": "n", "version": "1", "api": "1"})"), "a text api");
    expect(refused(R"({"id": "e", "name": "n", "version": "1", "api": 1,
                       "permissions": {"read": {"x": ["relative/path"]}}})"),
           "a relative read path");
    expect(refused(R"({"id": "e", "name": "n", "version": "1", "api": 1,
                       "permissions": {"read": {"x": ["{nowhere}/a"]}}})"),
           "an unknown token");
    expect(refused(R"({"id": "e", "name": "n", "version": "1", "api": 1,
                       "permissions": {"read": {"x": ["{xdg_data}/../.ssh"]}}})"),
           "a path that climbs out");
    expect(refused(R"({"id": "e", "name": "n", "version": "1", "api": 1,
                       "permissions": {"http": ["Example.COM/x"]}})"),
           "a host that is not a host name");
}

void expandsTokens() {
    // Absolute on this OS: "/home/p" has no drive on Windows.
    const std::filesystem::path home = std::filesystem::absolute("/home/p");
    const PathTokens tokens{.home = home,
                            .xdgData = home / ".local/share",
                            .xdgConfig = home / ".config",
                            .xdgCache = home / ".cache"};
    expect(expandPath("{xdg_data}/eden/play_time", tokens) == home / ".local/share/eden/play_time",
           "xdg_data");
    expect(expandPath("{xdg_cache}", tokens) == home / ".cache", "a token alone");
    expect(expandPath("{flatpak_data:dev.eden_emu.eden}/eden", tokens) ==
               home / ".var/app/dev.eden_emu.eden/data/eden",
           "a Flatpak app's data");
    expect(expandPath("{flatpak_cache:dev.eden_emu.eden}", tokens) ==
               home / ".var/app/dev.eden_emu.eden/cache",
           "a Flatpak app's cache");
    expect(!expandPath("{flatpak_data:../x}/y", tokens), "an app id with a slash");
    expect(!expandPath("{home}x", tokens), "a token run into a name");
    expect(!expandPath("{home}/x", PathTokens{}), "a token with no directory here");
}

} // namespace

int main() try {
    readsAWholeManifest();
    refusesWhatItCannotTrust();
    expandsTokens();
    std::printf("manifest: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
