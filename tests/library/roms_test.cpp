// The ROM source: systems by folder, a game folder's file, root discovery and emulator search,
// against a synthetic tree laid out like a real library.
#include "library/emulators.hpp"
#include "library/rom_systems.hpp"
#include "library/roms.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using iideck::library::Game;
using iideck::library::roms::Emulators;
using iideck::library::roms::EmulatorSearch;
using iideck::library::roms::gameFile;
using iideck::library::roms::Provider;
using iideck::library::roms::systemForFolder;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void write(const fs::path& path, std::size_t bytes = 1) {
    fs::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary};
    out << std::string(bytes, 'x');
}

void writeExecutable(const fs::path& path) {
    write(path);
    fs::permissions(path, fs::perms::owner_all);
}

struct Fixture {
    fs::path base = fs::temp_directory_path() / ("iideck-roms-test-" + std::to_string(getpid()));
    fs::path home = base / "home";
    fs::path mounts = base / "mnt";
    fs::path root = mounts / "Boy" / "ROM";

    Fixture() {
        fs::remove_all(base);
        write(root / "PS2" / "Black (USA).chd");
        write(root / "PSX CHD" / "Tomba! (USA).chd");
        write(root / "GameBoy" / "Final Fantasy Adventure (USA).gb");
        write(root / "GameBoy" / "Final Fantasy Adventure DX (USA).ips");
        write(root / "Switch" / "Kirby Star Allies" /
                  "Kirby Star Allies[01007E3006DDA000][US][v0].nsp",
              10);
        write(root / "Switch" / "Kirby Star Allies" /
                  "Kirby Star Allies[01007E3006DDA800][US][v327680].nsp",
              20);
        write(root / "Switch" / "Kirby Star Allies" / "ROMSIM.url");
        write(root / "Switch" / "Metroid Dread" / "Metroid Dread.nsp", 10);
        write(root / "Switch" / "Metroid Dread" / "Update 2.0.0.nsp", 30);
        write(root / "Switch" / "Super Mario Bros. Wonder [010015100B514000][v0][US].xci");
        write(root / "Wii U" / "Breath of the Wild" / "code" / "U-King.rpx");
        write(root / "Wii U" / "Breath of the Wild" / "content" / "Actor" / "a.pack");
        write(root / "PS4" / "CUSA00900" / "eboot.bin");
        write(root / "PS4" / "CUSA00900" / "sce_sys" / "param.sfo");
        write(root / "PS3" / "Time Crisis 4 (USA)" / "Time Crisis 4 (USA).iso");
        write(root / "PS3" / "Time Crisis 4 (USA)" / "README.TXT");
        write(root / "Android" / "game.apk");
        // A folder named like a root that holds no system is not one.
        write(mounts / "Other" / "roms" / "notes.txt");
    }

    ~Fixture() {
        fs::remove_all(base);
    }
};

const Game* find(const std::vector<Game>& games, const std::string& title) {
    const auto found = std::ranges::find(games, title, &Game::title);
    return found == games.end() ? nullptr : &*found;
}

void systemsByFolder() {
    expect(systemForFolder("PSX CHD") != nullptr && systemForFolder("PSX CHD")->key == "psx",
           "PSX CHD holds PlayStation games");
    expect(systemForFolder("Wii U")->key == "wiiu", "Wii U is matched without its space");
    expect(systemForFolder("GameBoy")->key == "gb", "GameBoy is the Game Boy");
    expect(systemForFolder("gamecube")->key == "gc", "names match in any case");
    expect(systemForFolder("Android") == nullptr, "Android is no emulated system");
}

void gameFiles(const Fixture& f) {
    const auto& sw = *systemForFolder("Switch");
    expect(gameFile(sw, f.root / "Switch" / "Kirby Star Allies")->filename() ==
               "Kirby Star Allies[01007E3006DDA000][US][v0].nsp",
           "a Switch folder starts its [v0] base, not the larger update");
    expect(gameFile(sw, f.root / "Switch" / "Metroid Dread")->filename() == "Metroid Dread.nsp",
           "a file named Update is not the base");
    expect(
        gameFile(*systemForFolder("Wii U"), f.root / "Wii U" / "Breath of the Wild")->filename() ==
            "U-King.rpx",
        "an extracted Wii U game starts its code/*.rpx");
    expect(gameFile(*systemForFolder("PS4"), f.root / "PS4" / "CUSA00900")->filename() ==
               "eboot.bin",
           "a PS4 folder starts eboot.bin");
    expect(gameFile(*systemForFolder("PS3"), f.root / "PS3" / "Time Crisis 4 (USA)")->filename() ==
               "Time Crisis 4 (USA).iso",
           "a PS3 folder starts its ISO");
    expect(!gameFile(*systemForFolder("GameBoy"),
                     f.root / "GameBoy" / "Final Fantasy Adventure DX (USA).ips"),
           "a patch is no game");
}

void rootsAreDiscovered(const Fixture& f) {
    const std::vector<fs::path> roots = iideck::library::roms::discoverRoots(f.home, {f.mounts});
    expect(roots.size() == 1 && roots.front() == f.root,
           "the drive's ROM folder is the one root; a roms folder without systems is not");
}

void emulatorsAreFound(const Fixture& f) {
    const fs::path bin = f.base / "bin";
    const fs::path appImages = f.base / "AppImages";
    const fs::path flatpaks = f.base / "flatpak";
    writeExecutable(bin / "dolphin-emu");
    writeExecutable(appImages / "eden_nightly.appimage");
    writeExecutable(appImages / "pcsx2.AppImage");
    fs::create_directories(flatpaks / "org.duckstation.DuckStation");
    const EmulatorSearch search{
        .executablePath = {bin}, .appImageDirs = {appImages}, .flatpakDirs = {flatpaks}};
    const Emulators emulators = Emulators::discover(search, {{"gb", {"mygb", "--rom"}}});

    const auto gc = emulators.launch("gc", "/g.rvz");
    expect(gc && gc->program == (bin / "dolphin-emu").string() &&
               gc->args == std::vector<std::string>{"-b", "-e", "/g.rvz"},
           "Dolphin on PATH runs GameCube games in batch mode");
    expect(emulators.launch("wii", "/w.rvz").has_value(), "and Wii games");
    const auto sw = emulators.launch("switch", "/s.nsp");
    expect(sw && sw->program == (appImages / "eden_nightly.appimage").string(),
           "an Eden AppImage runs Switch games");
    expect(emulators.launch("ps2", "/p.chd")->args.back() == "/p.chd",
           "PCSX2's AppImage, matched in any case, takes the game last");
    const auto psx = emulators.launch("psx", "/x.chd");
    expect(psx && psx->program == "flatpak" && psx->args[0] == "run" &&
               psx->args[1] == "org.duckstation.DuckStation",
           "an installed Flatpak runs through flatpak run");
    const auto gb = emulators.launch("gb", "/b.gb");
    expect(gb && gb->program == "mygb" && gb->args == std::vector<std::string>{"--rom", "/b.gb"},
           "an override without {rom} gets the game appended");
    expect(!emulators.launch("n3ds", "/c.3ds"), "nothing runs a system with no emulator");
}

void providerLists(const Fixture& f) {
    const fs::path bin = f.base / "bin2";
    writeExecutable(bin / "rpcs3");
    Provider provider{{f.root}, Emulators::discover(EmulatorSearch{.executablePath = {bin}}, {})};
    const std::vector<Game> games = provider.list();

    expect(games.size() == 9, "every game of every known system is listed once");
    const Game* kirby = find(games, "Kirby Star Allies");
    expect(kirby != nullptr && kirby->sourceId == "switch", "a game folder is one game");
    expect(kirby->launch.empty() &&
               kirby->unavailable == "no Nintendo Switch emulator found; install Eden or Ryujinx",
           "a game without an emulator names what to install");
    expect(find(games, "Super Mario Bros. Wonder") != nullptr, "dump tags leave the title");
    const Game* tc4 = find(games, "Time Crisis 4 (USA)");
    expect(tc4 != nullptr && !tc4->launch.empty() &&
               tc4->launch.args.back() ==
                   (f.root / "PS3" / "Time Crisis 4 (USA)" / "Time Crisis 4 (USA).iso").string(),
           "a game with an emulator starts its file");
    expect(tc4->processHint == "Time Crisis 4 (USA).iso", "the hint is the file's name");
    expect(find(games, "Tomba! (USA)")->sourceId == "psx", "PSX CHD games are PlayStation");
    expect(find(games, "Tomba! (USA)")->artworkKey == "Tomba! (USA)" &&
               tc4->artworkKey == "Time Crisis 4 (USA)" &&
               find(games, "Super Mario Bros. Wonder")->artworkKey ==
                   "Super Mario Bros. Wonder [010015100B514000][v0][US]",
           "artwork is looked up by the file or folder name, tags kept");
}

} // namespace

int main() {
    const Fixture fixture;
    systemsByFolder();
    gameFiles(fixture);
    rootsAreDiscovered(fixture);
    emulatorsAreFound(fixture);
    providerLists(fixture);
    std::printf("roms: all checks passed\n");
    return 0;
}
