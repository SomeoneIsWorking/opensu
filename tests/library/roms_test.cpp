// The ROM source: systems by folder, a game folder's file, root discovery and emulator search,
// against a synthetic tree laid out like a real library.
#include "library/emulators.hpp"
#include "library/rom_systems.hpp"
#include "library/roms.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <unistd.h>

#include "library/appimage_runtime.hpp"
#include "library/core_target.hpp"
#include "library/downloaded_emulators.hpp"
#include "library/romset_dat.hpp"
#include "library/romset_dats.hpp"
#include "library/romset_lists.hpp"
#include "support/test_main.hpp"
#include "support/zip_fixture.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::Game;
using opensu::library::roms::arcadeCoresOf;
using opensu::library::roms::checkingRomset;
using opensu::library::roms::coreTarget;
using opensu::library::roms::EmulatorCatalog;
using opensu::library::roms::EmulatorEntry;
using opensu::library::roms::Emulators;
using opensu::library::roms::EmulatorSearch;
using opensu::library::roms::EmulatorSetup;
using opensu::library::roms::gameFile;
using opensu::library::roms::holdsGames;
using opensu::library::roms::NameDb;
using opensu::library::roms::NameMap;
using opensu::library::roms::noRomsetMatch;
using opensu::library::roms::Provider;
using opensu::library::roms::readRoot;
using opensu::library::roms::RomsetDat;
using opensu::library::roms::RomsetDats;
using opensu::library::roms::RomsetLists;
using opensu::library::roms::systemForFile;
using opensu::library::roms::systemForFolder;
using opensu::test::zipfixture::buildZip;
using opensu::test::zipfixture::crcOf;
using opensu::test::zipfixture::FixtureEntry;

EmulatorCatalog shipped() {
    return EmulatorCatalog::shipped(OPENSU_ASSETS_DIR, "/home/p");
}

[[noreturn]] void fail(const char* what) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::exit(1);
}

template <class T> const T& need(const std::optional<T>& value, const char* what) {
    if (!value) {
        fail(what);
    }
    return *value;
}

void expect(bool condition, const char* what) {
    if (!condition) {
        fail(what);
    }
}

void write(const fs::path& path, std::size_t bytes = 1) {
    fs::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary};
    out << std::string(bytes, 'x');
}

/// A PARAM.SFO holding `strings` as UTF-8 values and one integer, laid out as PS4 dumps do.
void writeSfo(const fs::path& path,
              const std::vector<std::pair<std::string, std::string>>& strings) {
    const auto le = [](std::string& out, std::uint32_t value, int width) {
        for (int i = 0; i < width; ++i) {
            out.push_back(static_cast<char>((value >> (8 * i)) & 0xffU));
        }
    };
    std::vector<std::pair<std::string, std::string>> entries = strings;
    entries.emplace_back("APP_TYPE", std::string(4, '\0'));
    std::string keys;
    std::string data;
    std::string table;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const bool integer = i + 1 == entries.size();
        const std::string value = integer ? entries[i].second : entries[i].second + '\0';
        le(table, static_cast<std::uint32_t>(keys.size()), 2);
        le(table, integer ? 0x0404U : 0x0204U, 2);
        le(table, static_cast<std::uint32_t>(value.size()), 4);
        le(table, static_cast<std::uint32_t>(value.size()), 4);
        le(table, static_cast<std::uint32_t>(data.size()), 4);
        keys += entries[i].first + '\0';
        data += value;
    }
    std::string sfo{"\0PSF", 4};
    le(sfo, 0x0101, 4);
    const auto keyTable = static_cast<std::uint32_t>(20 + table.size());
    le(sfo, keyTable, 4);
    le(sfo, static_cast<std::uint32_t>(keyTable + keys.size()), 4);
    le(sfo, static_cast<std::uint32_t>(entries.size()), 4);
    fs::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary};
    out << sfo << table << keys << data;
}

/// A Wii disc whose header, game id `id`, starts at `at` behind the container's `magic`.
void writeWiiDisc(const fs::path& path, std::string_view magic, std::size_t at,
                  std::string_view id) {
    std::string bytes(at + 0x80, '\0');
    bytes.replace(0, magic.size(), magic);
    bytes.replace(at, id.size(), id);
    bytes.replace(at + 0x18, 4, "\x5D\x1C\x9E\xA3");
    fs::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary};
    out << bytes;
}

void writeExecutable(const fs::path& path) {
    write(path);
    fs::permissions(path, fs::perms::owner_all);
}

struct Fixture {
    fs::path base = fs::temp_directory_path() / ("opensu-roms-test-" + std::to_string(getpid()));
    fs::path home = base / "home";
    fs::path mounts = base / "mnt";
    fs::path root = mounts / "Boy" / "ROM";

    Fixture() {
        fs::remove_all(base);
        write(root / "PS2" / "Black (USA).chd");
        write(root / "Arcade" / "sf2.zip");
        write(root / "Arcade" / "unlisted.zip");
        write(root / "PSX CHD" / "Tomba! (USA).chd");
        write(root / "GameBoy" / "Final Fantasy Adventure (USA).gb");
        write(root / "GameBoy" / "Final Fantasy Adventure DX (USA).ips");
        write(root / "Switch" / "Kirby Star Allies" /
                  "Kirby Star Allies[01007E3006DDA000][US][v0].nsp",
              10);
        write(root / "Switch" / "Kirby Star Allies" /
                  "Kirby Star Allies[01007E3006DDA800][US][v327680].nsp",
              20);
        write(root / "Switch" / "Kirby Star Allies" /
                  "Kirby Star Allies Pack [01007E3006DDB001][v0].nsp",
              30);
        write(root / "Switch" / "Kirby Star Allies" / "ROMSIM.url");
        write(root / "Switch" / "Metroid Dread" / "Metroid Dread.nsp", 10);
        write(root / "Switch" / "Metroid Dread" / "Update 2.0.0.nsp", 30);
        write(root / "Switch" / "Super Mario Bros. Wonder [010015100B514000][v0][US].xci");
        write(root / "Wii U" / "Breath of the Wild" / "code" / "U-King.rpx");
        write(root / "Wii U" / "Breath of the Wild" / "content" / "Actor" / "a.pack");
        write(root / "PS4" / "CUSA00900" / "eboot.bin");
        writeSfo(root / "PS4" / "CUSA00900" / "sce_sys" / "param.sfo",
                 {{"TITLE", "Bloodborne\u2122"}, {"TITLE_ID", "CUSA00900"}});
        write(root / "PS4" / "CUSA00900" / "sce_sys" / "icon0.png");
        write(root / "PS4" / "CUSA01234" / "eboot.bin");
        write(root / "PS4" / "CUSA01234" / "sce_sys" / "param.sfo", 40);
        write(root / "PS3" / "Time Crisis 4 (USA)" / "Time Crisis 4 (USA).iso");
        write(root / "PS3" / "Time Crisis 4 (USA)" / "README.TXT");
        write(root / "Android" / "game.apk");
        writeWiiDisc(root / "Wii" / "Donkey Kong Country Returns (USA).rvz",
                     std::string_view{"RVZ\x01", 4}, 0x58, "SF8E01");
        writeWiiDisc(root / "Wii" / "Super Mario Galaxy (USA).iso", "", 0, "RMGE01");
        write(root / "Wii" / "Unread (USA).wbfs", 4);
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

/// The file `system` starts for the game at `entry`, found on the filesystem.
std::optional<fs::path> fileOf(const opensu::library::roms::RomSystem& system,
                               const fs::path& entry) {
    const opensu::library::roms::FileTree files;
    return gameFile(files, system, opensu::library::roms::FileTree::node(entry));
}

void gameFiles(const Fixture& f) {
    const auto& sw = *systemForFolder("Switch");
    expect(
        need(fileOf(sw, f.root / "Switch" / "Kirby Star Allies"), "Kirby has a file").filename() ==
            "Kirby Star Allies[01007E3006DDA000][US][v0].nsp",
        "a Switch folder starts its base title (id ending 000), not a larger update or add-on");
    expect(need(fileOf(sw, f.root / "Switch" / "Metroid Dread"), "Metroid has a file").filename() ==
               "Metroid Dread.nsp",
           "a file named Update is not the base");
    expect(need(fileOf(*systemForFolder("Wii U"), f.root / "Wii U" / "Breath of the Wild"),
                "Breath of the Wild has a file")
                   .filename() == "U-King.rpx",
           "an extracted Wii U game starts its code/*.rpx");
    expect(need(fileOf(*systemForFolder("PS4"), f.root / "PS4" / "CUSA00900"),
                "the PS4 folder has a file")
                   .filename() == "eboot.bin",
           "a PS4 folder starts eboot.bin");
    expect(need(fileOf(*systemForFolder("PS3"), f.root / "PS3" / "Time Crisis 4 (USA)"),
                "the PS3 folder has a file")
                   .filename() == "Time Crisis 4 (USA).iso",
           "a PS3 folder starts its ISO");
    expect(!fileOf(*systemForFolder("GameBoy"),
                   f.root / "GameBoy" / "Final Fantasy Adventure DX (USA).ips"),
           "a patch is no game");
}

void rootsAreDiscovered(const Fixture& f) {
    const std::vector<fs::path> roots = opensu::library::roms::discoverRoots(f.home, {f.mounts});
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
    const Emulators emulators = Emulators::discover(shipped(), search, {{"gb", {"mygb", "--rom"}}});

    const auto gc = emulators.launch("gc", "/g.rvz");
    expect(gc && gc->program == (bin / "dolphin-emu").string() &&
               gc->args == std::vector<std::string>{"-b", "-e", "/g.rvz"},
           "Dolphin on PATH runs GameCube games in batch mode");
    expect(emulators.launch("wii", "/w.rvz").has_value(), "and Wii games");
    const auto sw = emulators.launch("switch", "/s.nsp");
    expect(sw && sw->program == (appImages / "eden_nightly.appimage").string(),
           "an Eden AppImage runs Switch games");
    expect(need(emulators.launch("ps2", "/p.chd"), "PCSX2 runs PS2 games").args.back() == "/p.chd",
           "PCSX2's AppImage, matched in any case, takes the game last");
    const auto psx = emulators.launch("psx", "/x.chd");
    expect(psx && psx->program == "flatpak" && psx->args[0] == "run" &&
               psx->args[1] == "org.duckstation.DuckStation",
           "an installed Flatpak runs through flatpak run");
    const EmulatorSearch android{.androidApps = {"me.magnum.melonds"}};
    const auto nds =
        Emulators::discover(shipped(), android, {}).launch("nds", "content://p/document/d.nds");
    expect(nds &&
               nds->program == "me.magnum.melonds/me.magnum.melonds.ui.emulator.EmulatorActivity" &&
               nds->args == std::vector<std::string>{"-d", "content://p/document/d.nds"},
           "an installed Android app runs its system's games by intent");
    const auto gb = emulators.launch("gb", "/b.gb");
    expect(gb && gb->program == "mygb" && gb->args == std::vector<std::string>{"--rom", "/b.gb"},
           "an override without {rom} gets the game appended");
    expect(!emulators.launch("n3ds", "/c.3ds"), "nothing runs a system with no emulator");

    const auto options = emulators.options("switch", "/s.nsp");
    expect(options.size() == 4 && options[0].name == "Eden" && options[0].installed &&
               options[0].launch.args.back() == "/s.nsp" && options[1].name == "Ryujinx" &&
               !options[1].installed && options[1].launch.empty() && options[2].name == "Citron" &&
               options[3].name == "sudachi",
           "a system's options list every emulator, the installed one with its command");
    expect(emulators.chosenName("switch") == "Eden" && emulators.chosenName("n3ds").empty(),
           "the default is the first one found, and none when none is");
    const auto custom = emulators.options("gb", "/b.gb");
    expect(!custom.empty() && custom.front().name == "Custom" && custom.front().installed &&
               emulators.chosenName("gb") == "Custom",
           "a configured override is the first option and the default");
}

void providerLists(const Fixture& f) {
    const fs::path bin = f.base / "bin2";
    writeExecutable(bin / "rpcs3");
    const fs::path cache = f.base / "cache";
    const NameDb names = NameDb::under(cache);
    Provider provider{{f.root},
                      EmulatorSetup{shipped(), EmulatorSearch{.executablePath = {bin}}, {}},
                      names,
                      {},
                      {}};
    const std::vector<Game> before = provider.list(std::stop_token{});
    expect(find(before, "sf2") != nullptr && find(before, "unlisted") != nullptr,
           "an arcade short name is its own title until the listing is kept");

    std::string error;
    expect(names.save(NameMap{{"sf2", "Street Fighter II: The World Warrior (World 910522)"},
                              {"black", "Wrong System Entry"}},
                      error),
           "the arcade listing is kept");
    const std::vector<Game> games = provider.list(std::stop_token{});

    expect(games.size() == 15, "every game of every known system is listed once");
    const Game* returns = find(games, "Donkey Kong Country Returns");
    const Game* galaxy = find(games, "Super Mario Galaxy");
    const Game* unreadDisc = find(games, "Unread");
    expect(returns != nullptr && returns->titleId == "SF8E01" && galaxy != nullptr &&
               galaxy->titleId == "RMGE01",
           "a Wii disc's game id is read from its header, in an RVZ and a plain image");
    expect(unreadDisc != nullptr && unreadDisc->titleId.empty(),
           "a file without a Wii header has no game id");
    const Game* bloodborne = find(games, "Bloodborne");
    expect(bloodborne != nullptr && bloodborne->artworkKey == "Bloodborne" &&
               bloodborne->artwork == f.root / "PS4" / "CUSA00900" / "sce_sys" / "icon0.png",
           "a PS4 dump is titled by its param.sfo and shows its own icon");
    const Game* unread = find(games, "CUSA01234");
    expect(unread != nullptr && unread->artwork.empty(),
           "a PS4 dump whose param.sfo is unreadable keeps its folder name");
    const Game* sf2 = find(games, "Street Fighter II: The World Warrior");
    expect(sf2 != nullptr &&
               sf2->artworkKey == "Street Fighter II: The World Warrior (World 910522)",
           "an arcade short name is titled and keyed for box art by the listing's description");
    expect(find(games, "unlisted") != nullptr, "a short name the listing lacks keeps its name");
    expect(find(games, "Black") != nullptr, "the listing only titles arcade games");
    const Game* kirby = find(games, "Kirby Star Allies");
    expect(kirby != nullptr && kirby->sourceId == "switch", "a game folder is one game");
    expect(kirby->launch.empty() &&
               kirby->unavailable ==
                   "no Nintendo Switch emulator found; install Eden or Ryujinx or "
                   "Citron or sudachi",
           "a game without an emulator names what to install");
    expect(find(games, "Super Mario Bros. Wonder") != nullptr, "dump tags leave the title");
    const Game* tc4 = find(games, "Time Crisis 4");
    expect(tc4 != nullptr && !tc4->launch.empty() &&
               tc4->launch.args.back() ==
                   (f.root / "PS3" / "Time Crisis 4 (USA)" / "Time Crisis 4 (USA).iso").string(),
           "a game with an emulator starts its file");
    expect(tc4->processHint == "Time Crisis 4 (USA).iso", "the hint is the file's name");
    expect(find(games, "Tomba!")->sourceId == "psx", "PSX CHD games are PlayStation");
    expect(find(games, "Tomba!")->artworkKey == "Tomba! (USA)" &&
               tc4->artworkKey == "Time Crisis 4 (USA)" &&
               find(games, "Super Mario Bros. Wonder")->artworkKey ==
                   "Super Mario Bros. Wonder [010015100B514000][v0][US]",
           "artwork is looked up by the file or folder name, tags kept");
}

void looseFilesArePlaced(const Fixture& f) {
    const fs::path loose = f.base / "loose";
    write(loose / "Link Clock.gba");
    write(loose / "Super Metroid (Europe).sfc");
    write(loose / "Some Disc (USA).iso");
    write(loose / "Other Disc (USA).chd");
    write(loose / "sf2.zip");
    write(loose / "notes.txt");
    write(loose / "GBA" / "Minish Cap.gba");
    const fs::path empty = f.base / "empty";
    write(empty / "notes.txt");
    write(empty / "gba" / "readme.txt");

    for (const auto& system : opensu::library::roms::romSystems()) {
        for (const std::string_view extension : system.loose) {
            expect(std::ranges::find(system.extensions, extension) != system.extensions.end(),
                   "a loose extension is one of the system's game extensions");
            expect(systemForFile(std::string{"a"} + std::string{extension}) == &system,
                   "no two systems place a loose file by the same extension");
        }
    }
    expect(systemForFile("a.ISO") == nullptr && systemForFile("a.zip") == nullptr &&
               systemForFile("a.rvz") == nullptr,
           "an extension more than one console uses places nothing");

    const opensu::library::roms::FileTree files;
    const auto top = need(readRoot(files, loose), "the loose root reads");
    expect(top.loose.size() == 2 && top.folders.size() == 1 && top.leftOut == 3,
           "two loose games placed, one console folder, the iso, chd and zip left out");
    const auto bare = need(readRoot(files, empty), "the empty root reads");
    expect(!holdsGames(files, bare) && bare.leftOut == 0,
           "a root with an empty console folder and no game files holds no games");
    expect(holdsGames(files, top), "a root with loose games holds games");

    Provider provider{{loose},
                      EmulatorSetup{shipped(), EmulatorSearch{}, {}},
                      NameDb::under(f.base / "cache-loose"),
                      {},
                      {}};
    const std::vector<Game> games = provider.list(std::stop_token{});
    expect(games.size() == 3, "the loose games and the console folder's game are listed");
    const Game* clock = find(games, "Link Clock");
    expect(clock != nullptr && clock->sourceId == "gba" && clock->file == loose / "Link Clock.gba",
           "a loose .gba is a Game Boy Advance game");
    expect(find(games, "Super Metroid")->sourceId == "snes",
           "a loose .sfc is a Super Nintendo game");
    expect(find(games, "Minish Cap")->sourceId == "gba", "a console folder still works");
}

/// `value` as `size` little-endian bytes at `at`.
void putLittleEndian(std::string& bytes, std::size_t at, std::uint64_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        bytes[at + i] = static_cast<char>((value >> (8U * i)) & 0xFFU);
    }
}

/// An AppImage whose runtime is a minimal ELF; `classic` puts libfuse.so.2 in the runtime, as the
/// runtime that loads it does.
void writeAppImage(const fs::path& path, bool classic) {
    std::string bytes(4096, '\0');
    // ELF64 header: magic, ELFCLASS64, then e_shoff, e_shentsize and e_shnum.
    bytes[0] = '\x7F';
    bytes.replace(1, 3, "ELF");
    bytes[4] = 2;
    putLittleEndian(bytes, 0x28, 64, 8);
    putLittleEndian(bytes, 0x3A, 64, 2);
    putLittleEndian(bytes, 0x3C, 2, 2);
    if (classic) {
        bytes.replace(100, 12, "libfuse.so.2");
    } else {
        // Past the runtime: the filesystem image may name anything.
        bytes.replace(1000, 12, "libfuse.so.2");
    }
    fs::create_directories(path.parent_path());
    std::ofstream{path, std::ios::binary} << bytes;
    fs::permissions(path, fs::perms::owner_all);
}

void installedCoresFirst(const Fixture& f) {
    const fs::path cores = f.base / "arcade-cores";
    const auto order = [](const Emulators& emulators) {
        std::vector<std::string> ids;
        for (const EmulatorEntry* entry : arcadeCoresOf(emulators)) {
            ids.push_back(entry->id);
        }
        return ids;
    };
    const std::vector<std::string> catalog =
        order(Emulators::discover(shipped(), EmulatorSearch{}, {}));
    expect(catalog.size() > 2 && catalog.front() == "fbneo-core",
           "with nothing installed the catalog's order stands");
    write(cores / ("mame2010_libretro" + std::string{coreTarget.librarySuffix}));
    EmulatorSearch search;
    search.cores = opensu::library::roms::CoreSearch{{}, {cores}, {}};
    search.runner = cores / "opensu-retro";
    writeExecutable(search.runner);
    const std::vector<std::string> installed = order(Emulators::discover(shipped(), search, {}));
    expect(installed.front() == "mame2010-core" && installed[1] == "fbneo-core" &&
               installed.size() == catalog.size(),
           "an installed core's listing is asked for first, the rest in the catalog's order");
}

/// A hidden shell's runner is silent in a Gamescope and hidden elsewhere; the player's is neither.
void runnerModeWords(const Fixture& f) {
    using opensu::library::roms::RunnerMode;
    const fs::path cores = f.base / "mode-cores";
    write(cores / ("mame2010_libretro" + std::string{coreTarget.librarySuffix}));
    EmulatorSearch search;
    search.cores = opensu::library::roms::CoreSearch{{}, {cores}, {}};
    search.runner = cores / "opensu-retro";
    writeExecutable(search.runner);
    const auto words = [&search](RunnerMode mode) {
        search.runnerMode = mode;
        const Emulators emulators = Emulators::discover(shipped(), search, {});
        const opensu::library::roms::EmulatorInstall* install =
            emulators.installed("mame2010-core");
        return install != nullptr ? install->command : std::vector<std::string>{};
    };
    const auto has = [](const std::vector<std::string>& command, std::string_view word) {
        return std::ranges::find(command, word) != command.end();
    };
    const std::vector<std::string> player = words(RunnerMode::Player);
    const std::vector<std::string> silent = words(RunnerMode::Silent);
    const std::vector<std::string> hidden = words(RunnerMode::Hidden);
    expect(!player.empty() && !has(player, "--silent") && !has(player, "--hidden"),
           "the player's runner is started as it is");
    expect(has(silent, "--silent") && !has(silent, "--hidden"),
           "a hidden shell's runner in a Gamescope draws on its screen, silent");
    expect(has(hidden, "--hidden") && !has(hidden, "--silent"),
           "a hidden shell's runner elsewhere is hidden");
}

void biosIsNoGame(const Fixture& f) {
    const std::string program = "program rom";
    const std::string bios = "bios rom";
    const fs::path root = f.base / "bios-roms";
    const fs::path cache = f.base / "bios-cache";
    const auto zipped = [&root](const std::string& zip, const std::string& rom,
                                const std::string& data) {
        fs::create_directories(root / "Arcade");
        std::ofstream{root / "Arcade" / zip, std::ios::binary}
            << buildZip({FixtureEntry{rom, data, false, std::nullopt}});
    };
    zipped("game.zip", "p.1", program);
    zipped("system.zip", "b.1", bios);

    RomsetDat dat;
    dat.addXml(std::format(R"(<datafile>
	<game name="system" isbios="yes"><rom name="b.1" size="{0}" crc="{1:08x}"/></game>
	<game name="game" romof="system">
		<rom name="p.1" size="{2}" crc="{3:08x}"/>
		<rom name="b.1" merge="b.1" size="{0}" crc="{1:08x}"/>
	</game>
</datafile>)",
                           bios.size(), crcOf(bios), program.size(), crcOf(program)));
    RomsetLists lists{cache};
    const EmulatorCatalog catalog = shipped();
    std::string error;
    for (const EmulatorEntry& entry : catalog.entries()) {
        if (entry.serves("arcade") && !entry.romsets.empty()) {
            expect(lists.dats().save(entry, dat, 1, RomsetDats::Clock::now(), error),
                   "the listing is kept");
        }
    }
    for (int scan = 0; scan < 2; ++scan) {
        Provider provider{{root},
                          EmulatorSetup{shipped(), EmulatorSearch{}, {}},
                          NameDb::under(cache),
                          &lists,
                          {}};
        const std::vector<Game> games = provider.list(std::stop_token{});
        expect(find(games, "system") == nullptr, "a BIOS's zip is not listed");
        const Game* game = find(games, "game");
        expect(game != nullptr && !game->romset.empty() && game->romset != checkingRomset &&
                   game->romset != noRomsetMatch,
               "a game beside it still matches through it");
        expect(game != nullptr && game->romsetName == "game",
               "the game carries its set's name for the launch's listing lookup");
    }
}

void downloadsMergeWithDetected(const Fixture& f) {
    using opensu::library::roms::DownloadedEmulator;
    using opensu::library::roms::DownloadedEmulators;
    using opensu::library::roms::FuseSupport;
    using opensu::library::roms::InstallSource;
    const fs::path bin = f.base / "merge-bin";
    const fs::path appImages = f.base / "merge-AppImages";
    const fs::path flatpaks = f.base / "merge-flatpak";
    const fs::path downloads = f.base / "merge-emulators";
    writeExecutable(bin / "mgba-qt");
    const DownloadedEmulators store{downloads};
    const fs::path ours = store.versionDir("mgba", "42") / "mGBA-0.10.5-appimage-x64.appimage";
    writeAppImage(ours, false);
    std::string error;
    expect(store.install(DownloadedEmulator{"mgba", "0.10.5", "42", ours, "ab", 4096,
                                            opensu::library::roms::Verification::Observed, "u"},
                         error),
           "the download is recorded");
    writeAppImage(appImages / "PPSSPP-v1.20-anylinux-x86_64.AppImage", true);
    writeAppImage(appImages / "Cemu-2.6-x86_64.AppImage", false);
    const fs::path metainfo =
        flatpaks / "app.xemu.xemu" / "current" / "active" / "files" / "share" / "metainfo";
    fs::create_directories(metainfo);
    std::ofstream{metainfo / "app.xemu.xemu.metainfo.xml"}
        << R"(<component><releases><release version="0.8.136" date="2026-01-01"/>)"
        << R"(<release version="0.8.100"/></releases></component>)";

    EmulatorSearch search{.executablePath = {bin},
                          .appImageDirs = {appImages},
                          .flatpakDirs = {flatpaks},
                          .downloads = downloads,
                          .fuse = FuseSupport{.libfuse2 = false, .distro = "fedora"}};
    const Emulators found = Emulators::discover(shipped(), search, {});

    const auto* mgba = found.installed("mgba");
    expect(mgba != nullptr && mgba->source == InstallSource::Downloaded &&
               mgba->version == "0.10.5" && mgba->where == ours.string(),
           "openSU's download is the mGBA that runs, ahead of the package on PATH");
    const auto gba = found.launch("gba", "/g.gba");
    expect(gba && gba->program == ours.string() &&
               gba->args == std::vector<std::string>{"-f", "/g.gba"},
           "and starts with the catalog's words");

    const auto* ppsspp = found.installed("ppsspp");
    expect(ppsspp != nullptr && ppsspp->source == InstallSource::AppImage &&
               ppsspp->problem.find("sudo dnf install fuse-libs") != std::string::npos,
           "an AppImage whose runtime needs a missing libfuse2 names the install command");
    expect(!found.launch("psp", "/p.iso"), "and runs nothing");
    const auto psp = found.options("psp", "/p.iso");
    expect(psp.size() == 2 && !psp[0].installed && !psp[0].problem.empty() && psp[0].downloadable &&
               psp[0].id == "ppsspp" && psp[1].id == "ppsspp-core",
           "its option says why it cannot start and that it can be downloaded, the core after it");

    const auto* cemu = found.installed("cemu");
    expect(cemu != nullptr && cemu->problem.empty(),
           "a runtime with FUSE inside starts without libfuse2");
    const auto* xemu = found.installed("xemu");
    expect(xemu != nullptr && xemu->source == InstallSource::Flatpak && xemu->version == "0.8.136",
           "a Flatpak's version is its newest release in the metainfo");
    expect(
        opensu::library::roms::needsLibfuse2(appImages / "PPSSPP-v1.20-anylinux-x86_64.AppImage") &&
            !opensu::library::roms::needsLibfuse2(ours),
        "only the runtime is read for libfuse.so.2");
    expect(opensu::library::roms::libfuse2Install("ubuntu debian").starts_with("sudo apt"),
           "Debian family gets apt");
}

} // namespace

int main() try {
    const Fixture fixture;
    systemsByFolder();
    gameFiles(fixture);
    rootsAreDiscovered(fixture);
    emulatorsAreFound(fixture);
    providerLists(fixture);
    looseFilesArePlaced(fixture);
    biosIsNoGame(fixture);
    installedCoresFirst(fixture);
    runnerModeWords(fixture);
    downloadsMergeWithDetected(fixture);
    std::printf("roms: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
