// The Epic source against a stub `legendary`: owned titles with install state, DLC dropped,
// and the missing and signed-out cases.
#include "library/epic.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using opensu::library::Game;
using opensu::library::SourceAbsent;
using opensu::library::epic::installFailure;
using opensu::library::epic::Provider;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

fs::path writeStub(const fs::path& dir, const std::string& name, const std::string& body) {
    fs::create_directories(dir);
    const fs::path path = dir / name;
    std::ofstream{path} << "#!/bin/sh\n" << body;
    fs::permissions(path, fs::perms::owner_all);
    return path;
}

const Game* find(const std::vector<Game>& games, const std::string& id) {
    const auto it = std::ranges::find(games, id, &Game::id);
    return it == games.end() ? nullptr : &*it;
}

/// `legendary install` failure lines, as legendary 0.20.35 logs them (cli.py).
void testInstallOutput() {
    expect(installFailure("[cli] ERROR: Login failed! Cannot continue with download process.") ==
               "Login failed! Cannot continue with download process.",
           "an error line is the reason");
    expect(installFailure("[cli] CRITICAL: Installation cannot proceed, exiting.") ==
               "Installation cannot proceed, exiting.",
           "a critical line is the reason");
    expect(installFailure(" ! Failure: Not enough available disk space") ==
               "Not enough available disk space",
           "a failed requirement is the reason");
    expect(!installFailure("[cli] INFO: Install size: 2048.00 MiB"), "an info line is no failure");
    expect(!installFailure("The ERROR: word alone"), "text mentioning an error is no failure");
}

} // namespace

int main() {
    testInstallOutput();
    const fs::path dir = fs::path{OPENSU_TEST_SCRATCH} / ("epic-" + std::to_string(getpid()));
    fs::remove_all(dir);

    const fs::path legendary = writeStub(dir, "legendary", R"(echo "log line" >&2
case "$1" in
list) echo '[{"app_name":"Fortnite","app_title":"Fortnite","metadata":{"keyImages":[
                {"type":"Thumbnail","url":"https://cdn.example/fn-thumb.jpg"},
                {"type":"DieselGameBox","url":"https://cdn.example/fn-wide.jpg"},
                {"type":"DieselGameBoxTall","url":"https://cdn.example/fn-tall.jpg"}]}},
             {"app_name":"Owned","app_title":"Owned Game","metadata":{"keyImages":[
                {"type":"DieselGameBoxLogo","url":"https://cdn.example/logo.png"},
                {"type":"Thumbnail","url":"https://cdn.example/owned-thumb.jpg"}]}},
             {"app_name":"Extra","app_title":"Extra Pack"},
             {"app_name":"Untitled","app_title":""},
             {"app_name":"Spaced","app_title":"Spaced","metadata":{"keyImages":[
                {"type":"DieselGameBoxTall","url":"https://cdn.example/public testing 1200 x 1600.jpg"}]}}]' ;;
list-installed) echo '[{"app_name":"Fortnite","title":"Fortnite","install_path":"/games/Fortnite","is_dlc":false},
                       {"app_name":"Extra","title":"Extra Pack","install_path":"/games/Extra","is_dlc":true}]' ;;
*) exit 2 ;;
esac
)");
    const std::vector<Game> games = Provider{legendary.string()}.list();
    expect(games.size() == 4, "every owned title but the installed DLC is listed");
    const Game* fortnite = find(games, "epic:Fortnite");
    expect(fortnite != nullptr && fortnite->installed, "an installed title is installed");
    expect(fortnite->processHint == "Fortnite", "its install folder names its process");
    const Game* owned = find(games, "epic:Owned");
    expect(owned != nullptr && !owned->installed, "an owned title not installed is listed");
    expect(owned->title == "Owned Game", "the title is legendary's app_title");
    expect(owned->launch.args == std::vector<std::string>{"launch", "Owned"},
           "it launches through legendary");
    expect(fortnite->artworkUrl == "https://cdn.example/fn-tall.jpg",
           "the portrait box is preferred");
    expect(owned->artworkUrl == "https://cdn.example/owned-thumb.jpg",
           "the thumbnail is the last resort and a logo is never used");
    const Game* spaced = find(games, "epic:Spaced");
    expect(spaced != nullptr &&
               spaced->artworkUrl == "https://cdn.example/public%20testing%201200%20x%201600.jpg",
           "spaces in an image URL are encoded, since curl refuses them");
    const Game* untitled = find(games, "epic:Untitled");
    expect(untitled != nullptr && untitled->title == "Untitled", "no title falls back to the id");
    expect(untitled->artworkUrl.empty(), "a title without key images has no artwork URL");

    const fs::path signedOut = writeStub(dir, "signed-out", "exit 1\n");
    bool threw = false;
    try {
        static_cast<void>(Provider{signedOut.string()}.list());
    } catch (const SourceAbsent&) {
        expect(false, "signed out is not absent");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect(threw, "signed out is an error");

    bool absent = false;
    try {
        static_cast<void>(Provider{(dir / "missing").string()}.list());
    } catch (const SourceAbsent&) {
        absent = true;
    }
    expect(absent, "a missing legendary is absent");

    fs::remove_all(dir);
    return 0;
}
