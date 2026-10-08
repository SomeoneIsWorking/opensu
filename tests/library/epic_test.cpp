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
using iideck::library::Game;
using iideck::library::SourceAbsent;
using iideck::library::epic::Provider;

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

} // namespace

int main() {
    const fs::path dir = fs::path{IIDECK_TEST_SCRATCH} / ("epic-" + std::to_string(getpid()));
    fs::remove_all(dir);

    const fs::path legendary = writeStub(dir, "legendary", R"(echo "log line" >&2
case "$1" in
list) echo '[{"app_name":"Fortnite","app_title":"Fortnite","metadata":{}},
             {"app_name":"Owned","app_title":"Owned Game"},
             {"app_name":"Extra","app_title":"Extra Pack"},
             {"app_name":"Untitled","app_title":""}]' ;;
list-installed) echo '[{"app_name":"Fortnite","title":"Fortnite","install_path":"/games/Fortnite","is_dlc":false},
                       {"app_name":"Extra","title":"Extra Pack","install_path":"/games/Extra","is_dlc":true}]' ;;
*) exit 2 ;;
esac
)");
    const std::vector<Game> games = Provider{legendary.string()}.list();
    expect(games.size() == 3, "every owned title but the installed DLC is listed");
    const Game* fortnite = find(games, "epic:Fortnite");
    expect(fortnite != nullptr && fortnite->installed, "an installed title is installed");
    expect(fortnite->processHint == "Fortnite", "its install folder names its process");
    const Game* owned = find(games, "epic:Owned");
    expect(owned != nullptr && !owned->installed, "an owned title not installed is listed");
    expect(owned->title == "Owned Game", "the title is legendary's app_title");
    expect(owned->launch.args == std::vector<std::string>{"launch", "Owned"},
           "it launches through legendary");
    const Game* untitled = find(games, "epic:Untitled");
    expect(untitled != nullptr && untitled->title == "Untitled", "no title falls back to the id");

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
