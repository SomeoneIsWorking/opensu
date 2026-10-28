// What a picked game file is: clues from the files around it (a version resource, steam_appid.txt,
// folder and file names) and the Steam store app each matches. The folder and file names are the
// real ones of games on the maintainer's disk; the store answers are Steam's for those names.
#include "library/game_identify.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "library/titles.hpp"
#include "support/scratch_dir.hpp"

namespace {

namespace fs = std::filesystem;
namespace identify = opensu::library::identify;
using opensu::library::own::Kind;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void put(std::string& bytes, std::size_t at, std::uint32_t value, std::size_t width) {
    if (bytes.size() < at + width) {
        bytes.resize(at + width);
    }
    for (std::size_t i = 0; i < width; ++i) {
        bytes[at + i] = static_cast<char>((value >> (8 * i)) & 0xFFU);
    }
}

std::string utf16(const std::string& text) {
    std::string out;
    for (const char c : text) {
        out += c;
        out += '\0';
    }
    return out + std::string(2, '\0');
}

/// A PE32+ file whose one resource is a version block holding `key` = `value`.
std::string programWith(const std::string& key, const std::string& value) {
    std::string block = std::string(6, '\0') + utf16(key);
    block.resize((block.size() + 3) & ~std::size_t{3});
    const std::string text = utf16(value);
    block += text;
    put(block, 0, static_cast<std::uint32_t>(block.size()), 2);
    put(block, 2, static_cast<std::uint32_t>(text.size() / 2), 2);
    put(block, 4, 1, 2);

    constexpr std::uint32_t rva = 0x1000;
    std::string rsrc;
    // Three directory levels (type, name, language), each with one id entry, then the data entry.
    put(rsrc, 14, 1, 2);
    put(rsrc, 16, 16, 4);
    put(rsrc, 20, 0x80000000U | 0x18U, 4);
    put(rsrc, 0x18 + 14, 1, 2);
    put(rsrc, 0x18 + 16, 1, 4);
    put(rsrc, 0x18 + 20, 0x80000000U | 0x30U, 4);
    put(rsrc, 0x30 + 14, 1, 2);
    put(rsrc, 0x30 + 16, 0x409, 4);
    put(rsrc, 0x30 + 20, 0x48, 4);
    put(rsrc, 0x48, rva + 0x58, 4);
    put(rsrc, 0x4C, static_cast<std::uint32_t>(block.size()), 4);
    rsrc.resize(0x58);
    rsrc += block;

    std::string exe = "MZ";
    put(exe, 0x3C, 0x40, 4);
    put(exe, 0x40, 0x00004550U, 4);
    put(exe, 0x40 + 4, 0x8664, 2);
    put(exe, 0x40 + 6, 1, 2);
    put(exe, 0x40 + 20, 240, 2);
    put(exe, 0x40 + 24, 0x20B, 2);
    put(exe, 0x40 + 24 + 112 + 16, rva, 4);
    put(exe, 0x40 + 24 + 112 + 20, static_cast<std::uint32_t>(rsrc.size()), 4);
    const std::size_t section = 0x40 + 24 + 240;
    exe.resize(section + 40);
    exe.replace(section, 5, ".rsrc");
    put(exe, section + 8, static_cast<std::uint32_t>(rsrc.size()), 4);
    put(exe, section + 12, rva, 4);
    put(exe, section + 16, static_cast<std::uint32_t>(rsrc.size()), 4);
    put(exe, section + 20, 0x200, 4);
    exe.resize(0x200);
    return exe + rsrc;
}

fs::path file(const fs::path& path, const std::string& bytes = "MZ") {
    fs::create_directories(path.parent_path());
    std::ofstream{path, std::ios::binary} << bytes;
    return path;
}

/// Steam's store search as it answered for these names: a term finds the apps whose titles
/// contain it, punctuation aside.
std::vector<identify::StoreApp> store(const std::string& term) {
    const std::vector<identify::StoreApp> apps{
        {"1245620", "ELDEN RING"},
        {"2622380", "ELDEN RING NIGHTREIGN"},
        {"1627720", "Lies of P"},
        {"2848330", "Lies of P: Overture"},
        {"1903340", "Clair Obscur: Expedition 33"},
        {"3228520", "Clair Obscur: Expedition 33 \xE2\x80\x93 Deluxe Edition Upgrade"},
        {"2050650", "Resident Evil 4"},
        {"254700", "Resident Evil 4 (2005)"},
        {"1599660", "Sackboy\xE2\x84\xA2: A Big Adventure"},
        {"1177980", "Little Kitty, Big City"},
        {"1262560", "Need for Speed\xE2\x84\xA2 Most Wanted"},
    };
    const std::string key = opensu::library::titleKey(term);
    std::vector<identify::StoreApp> found;
    for (const identify::StoreApp& app : apps) {
        if (!key.empty() && opensu::library::titleKey(app.title).find(key) != std::string::npos) {
            found.push_back(app);
        }
    }
    return found;
}

void versionResourceIsRead(const fs::path& dir) {
    const fs::path elden = file(dir / "eldenring.exe", programWith("ProductName", "ELDEN RING"));
    expect(identify::versionName(elden) == "ELDEN RING", "ProductName from the version resource");
    const fs::path bootstrap =
        file(dir / "LOP.exe", programWith("ProductName", "BootstrapPackagedGame"));
    expect(identify::versionName(bootstrap).empty(), "an engine's placeholder name is no name");
    expect(identify::versionName(file(dir / "XMen2.exe")).empty(), "no resource, no name");
}

void fileNamesAreTitles() {
    expect(identify::fileTitle("Expedition33_Steam.exe") == "Expedition33" &&
               identify::fileTitle("SandFall-Win64-Shipping.exe") == "SandFall" &&
               identify::fileTitle("nebula-reset.appimage") == "Nebula Reset" &&
               identify::fileTitle("Little Kitty, Big City.exe") == "Little Kitty, Big City" &&
               identify::fileTitle("start.sh").empty(),
           "build words dropped, separators spaced, a launcher name is no title");
}

void gamesAreIdentified(const fs::path& games) {
    struct Case {
        fs::path file;
        std::string title;
        std::string appId;
        Kind kind;
    };
    const fs::path elden =
        file(games / "ELDEN RING/Game/eldenring.exe", programWith("ProductName", "ELDEN RING"));
    const fs::path re4 =
        file(games / "Resident Evil 4/re4.exe", programWith("ProductName", "Resident Evil 4"));
    file(games / "Resident Evil 4/steam_appid.txt", "2050650\n");
    const fs::path nebula = file(games / "AppImages/nebula-reset.appimage", "ELF");
    fs::permissions(nebula, fs::perms::owner_all);
    const std::vector<Case> cases{
        {elden, "ELDEN RING", "1245620", Kind::Windows},
        {file(games / "Lies of P/LOP.exe", programWith("ProductName", "BootstrapPackagedGame")),
         "Lies of P", "1627720", Kind::Windows},
        {file(games / "Clair Obscur - Expedition 33/Sandfall/Binaries/Win64/"
                      "SandFall-Win64-Shipping.exe",
              programWith("ProductName", "Expedition 33")),
         "Clair Obscur: Expedition 33", "1903340", Kind::Windows},
        {re4, "Resident Evil 4", "2050650", Kind::Windows},
        {file(games / "Sackboy - A Big Adventure/Sackboy.exe",
              programWith("ProductName", "Sackboy")),
         "Sackboy: A Big Adventure", "1599660", Kind::Windows},
        {file(games / "Little Kitty, Big City/Little Kitty, Big City.exe"),
         "Little Kitty, Big City", "1177980", Kind::Windows},
        {file(games / "X-Men Legends II/XMen2.exe"), "X-Men Legends II", "", Kind::Windows},
        {file(games / "Need For Speed Most Wanted Remaster Edition/speed.exe"),
         "Need For Speed Most Wanted Remaster Edition", "", Kind::Windows},
        {nebula, "Nebula Reset", "", Kind::AppImage},
    };
    for (const Case& each : cases) {
        const identify::Guess guess = identify::identify(each.file, &store);
        if (guess.title != each.title || guess.steamAppId != each.appId ||
            guess.kind != each.kind) {
            std::fprintf(stderr, "%s: %s (%s) from %s\n", each.file.string().c_str(),
                         guess.title.c_str(), guess.steamAppId.c_str(), guess.from.c_str());
        }
        expect(guess.title == each.title && guess.steamAppId == each.appId &&
                   guess.kind == each.kind,
               "each game is what its files and the store say");
    }

    const identify::Guess offline = identify::identify(elden, [](const std::string&) {
        return std::vector<identify::StoreApp>{};
    });
    expect(offline.title == "ELDEN RING" && offline.steamAppId.empty() &&
               offline.from == "the folder's name",
           "without the store, the best clue is the title");
    const identify::Guess appIdOnly = identify::identify(re4, [](const std::string&) {
        return std::vector<identify::StoreApp>{};
    });
    expect(appIdOnly.steamAppId == "2050650", "steam_appid.txt gives the app without the store");
}

void storeAnswersAreRead() {
    const std::vector<identify::StoreApp> apps = identify::parseStoreSearch(
        R"({"total":2,"items":[{"type":"app","name":"Lies of P","id":1627720},)"
        R"({"type":"sub","name":"Bundle","id":5},{"type":"app","name":"Lies of P: Overture",)"
        R"("id":2848330}]})");
    expect(apps == std::vector<identify::StoreApp>{{"1627720", "Lies of P"},
                                                   {"2848330", "Lies of P: Overture"}},
           "apps only, ids as text");
    expect(identify::parseStoreSearch("<html>").empty(), "a page that is not an answer is none");
    expect(identify::storeSearchUrl("Clair Obscur - Expedition 33") ==
               "https://store.steampowered.com/api/storesearch/"
               "?term=Clair%20Obscur%20Expedition%2033&l=english&cc=US",
           "punctuation is dropped from the term, which Steam's search needs");
}

} // namespace

int main() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "game-identify"};
    versionResourceIsRead(scratch.path() / "programs");
    fileNamesAreTitles();
    gamesAreIdentified(scratch.path() / "Games");
    storeAnswersAreRead();
    std::printf("game_identify: all checks passed\n");
    return 0;
}
