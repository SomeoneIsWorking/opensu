// The Steam source, exercised against a synthetic installation.
//
// A real tree with one installed game, one Steam component, one known-but-absent
// game, artwork in the modern and legacy layouts, a user profile, and a second
// library folder reached both directly and through the install root.
#include "library/steam.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

namespace fs = std::filesystem;
using iideck::library::Game;
using iideck::library::steam::Library;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void write(const fs::path& path, const std::string& body) {
    fs::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary};
    out << body;
}

struct Fixture {
    fs::path base;
    fs::path root;
    fs::path extra;

    Fixture() {
        base = fs::temp_directory_path() / "iideck-steam-test";
        fs::remove_all(base);
        root = base / "Steam";
        extra = base / "Games";

        fs::create_directories(root / "steamapps" / "common" / "Portal 2");
        fs::create_directories(root / "config" / "grid");
        fs::create_directories(root / "appcache" / "librarycache" / "440");
        fs::create_directories(root / "userdata" / "1234567" / "config");
        fs::create_directories(extra / "steamapps" / "common" / "Deep Game");

        // The install root is also listed as library "0", which is how Steam
        // records it; it must not be read twice.
        write(root / "steamapps" / "libraryfolders.vdf",
              "\"libraryfolders\"\n"
              "{\n"
              "\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" +
                  root.string() +
                  "\"\n\t}\n"
                  "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"" +
                  extra.string() +
                  "\"\n"
                  "\t\t\"contentid\"\t\t\"8019845413043883551\"\n\t}\n"
                  "}\n");

        write(root / "steamapps" / "appmanifest_440.acf",
              "\"AppState\"\n{\n\t\"appid\"\t\t\"440\"\n\t\"name\"\t\t\"Portal 2\"\n"
              "\t\"installdir\"\t\t\"Portal 2\"\n\t\"StateFlags\"\t\t\"4\"\n}\n");

        // Known to Steam, but its directory is gone: not installed.
        write(root / "steamapps" / "appmanifest_620.acf",
              "\"AppState\"\n{\n\t\"appid\"\t\t\"620\"\n\t\"name\"\t\t\"Portal 2\"\n"
              "\t\"installdir\"\t\t\"Portal 2 (Missing)\"\n\t\"StateFlags\"\t\t\"1026\"\n}\n");

        // One of Steam's own components, which is not a game.
        write(root / "steamapps" / "appmanifest_2805730.acf",
              "\"AppState\"\n{\n\t\"appid\"\t\t\"2805730\"\n\t\"name\"\t\t\"Proton 9.0\"\n"
              "\t\"installdir\"\t\t\"Proton 9.0 (Beta)\"\n\t\"StateFlags\"\t\t\"4\"\n}\n");

        write(extra / "steamapps" / "appmanifest_999.acf",
              "\"AppState\"\n{\n\t\"appid\"\t\t\"999\"\n\t\"name\"\t\t\"Deep Game\"\n"
              "\t\"installdir\"\t\t\"Deep Game\"\n\t\"StateFlags\"\t\t\"4\"\n}\n");

        write(root / "appcache" / "librarycache" / "440" / "library_600x900.jpg", "portrait");
        write(root / "appcache" / "librarycache" / "440" / "library_hero.jpg", "landscape");
        // 620 only has the legacy grid image.
        write(root / "config" / "grid" / "620p.jpg", "legacy");
        // 999 has none.

        write(root / "userdata" / "1234567" / "config" / "localconfig.vdf",
              "\"UserLocalConfigStore\"\n{\n\t\"Software\"\n\t{\n\t\t\"Valve\"\n\t\t{\n"
              "\t\t\t\"Steam\"\n\t\t\t{\n\t\t\t\t\"Apps\"\n\t\t\t\t{\n"
              "\t\t\t\t\t\"440\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"LastPlayed\"\t\t\"1700000000\"\n"
              "\t\t\t\t\t\t\"Playtime\"\t\t\"145\"\n\t\t\t\t\t}\n\t\t\t\t}\n"
              "\t\t\t\t\"Favorites\"\n\t\t\t\t{\n\t\t\t\t\t\"440\"\t\t\"1\"\n\t\t\t\t}\n"
              "\t\t\t}\n\t\t}\n\t}\n}\n");
    }

    ~Fixture() {
        fs::remove_all(base);
    }
};

const Game* find(const std::vector<Game>& games, const std::string& id) {
    for (const Game& game : games) {
        if (game.id == id) {
            return &game;
        }
    }
    return nullptr;
}

} // namespace

int main() {
    Fixture fixture;

    const Library library = Library::discover(fixture.base / "", {fixture.root});
    expect(library.roots().size() == 1, "one install root discovered");

    // The install root is also library "0", so folder discovery must return it
    // once rather than twice.
    const std::vector<iideck::library::steam::LibraryFolder> folders = library.libraryFolders();
    expect(folders.size() == 2, "two library folders, the root and the extra one");
    int rootCount = 0;
    for (const auto& folder : folders) {
        rootCount += folder.path == fixture.root ? 1 : 0;
    }
    expect(rootCount == 1, "the install root appears once");

    iideck::library::steam::Provider provider{library};
    const std::vector<Game> games = provider.list();

    // Three games: the two manifests in the root that are games, plus the extra
    // library's. The Steam component is not a game.
    expect(games.size() == 3, "three games listed");
    expect(find(games, "steam:2805730") == nullptr, "a Steam component is not listed");

    const Game* portal = find(games, "steam:440");
    expect(portal != nullptr, "Portal 2 listed");
    expect(portal->installed, "Portal 2 is installed");
    expect(portal->title == "Portal 2", "Portal 2 named");
    expect(portal->playtimeMinutes == 145, "playtime read from the user profile");
    expect(portal->favourite, "favourite read from the user profile");
    expect(portal->lastPlayed.has_value(), "last played read");
    expect(portal->launch.program == "steam", "launch program");
    expect(portal->launch.args.size() == 3 && portal->launch.args[0] == "-silent" &&
               portal->launch.args[1] == "-applaunch" && portal->launch.args[2] == "440",
           "launch args");
    expect(portal->processHint == std::string{"AppId=440"} + std::string(1, '\0'),
           "process hint is Steam's whole AppId argument");
    expect(!portal->artwork.empty() && fs::exists(portal->artwork),
           "modern portrait artwork found");
    expect(!portal->artworkWide.empty() && fs::exists(portal->artworkWide),
           "modern landscape artwork found");

    const Game* missing = find(games, "steam:620");
    expect(missing != nullptr, "known-but-absent game listed");
    expect(!missing->installed, "absent directory means not installed");
    expect(!missing->artwork.empty(), "legacy grid artwork found");

    const Game* deep = find(games, "steam:999");
    expect(deep != nullptr, "game from the extra library listed");
    expect(deep->artwork.empty(), "a game with no artwork reports none");

    // The install root is listed as library 0, so nothing may appear twice.
    int seen = 0;
    for (const Game& game : games) {
        seen += game.id == "steam:440" ? 1 : 0;
    }
    expect(seen == 1, "the install root is not read twice");

    // A root that does not exist is not an install.
    const Library missingRoot = Library::discover(fixture.base, {fixture.base / "nope"});
    expect(missingRoot.roots().empty(), "a missing root is not an install root");

    std::printf("steam: all checks passed\n");
    return 0;
}