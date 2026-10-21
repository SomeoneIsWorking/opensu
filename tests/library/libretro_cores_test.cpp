// Built-in emulators' libretro cores through the shipping code, against a fake buildbot: the
// listing read, the lookup order, a download checked against the listed CRC-32, a mismatch and an
// off-host redirect refused with nothing kept, removal, and a found core as a ROM's emulator.
#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "library/emulators.hpp"
#include "library/libretro_cores.hpp"
#include "support/buildbot_server.hpp"
#include "support/scratch_dir.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu::library::roms;
using opensu::library::EmulatorOption;
using opensu::library::LaunchSpec;
using opensu::test::expect;
using opensu::test::need;

const opensu::net::WebClient::Progress anyProgress = [](std::uint64_t, std::uint64_t) {
    return true;
};

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void put(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << bytes;
}

/// The files left in `dir`, by name, sorted.
std::vector<std::string> namesIn(const fs::path& dir) {
    std::vector<std::string> names;
    for (const fs::directory_entry& entry : fs::directory_iterator{dir}) {
        names.push_back(entry.path().filename().string());
    }
    std::ranges::sort(names);
    return names;
}

void listingIsRead() {
    const std::string index = "2026-09-01 0badf00d 2048_libretro.so.zip\n"
                              "2026-10-01\tA1B2C3D4 mgba_libretro.so.zip\r\n"
                              "2026-10-01 nothex snes9x_libretro.so.zip\n";
    std::string error;
    const CoreBuild build =
        need(coreBuildIn(index, "mgba_libretro", "https://bb/n/", error), "a listed core is read");
    expect(build == CoreBuild{"mgba_libretro", "2026-10-01", 0xa1b2c3d4,
                              "https://bb/n/mgba_libretro.so.zip"},
           "with its date, CRC-32 and zip URL");
    expect(!coreBuildIn(index, "mesen_libretro", "https://bb/n/", error) &&
               error.find("does not list") != std::string::npos,
           "a core not listed says so");
    expect(!coreBuildIn(index, "snes9x_libretro", "https://bb/n/", error) &&
               error.find("CRC-32") != std::string::npos,
           "a malformed line is refused");
}

void lookupOrder(const fs::path& root) {
    const CoreSearch search{root / "ours", {root / "retroarch", root / "system"}, {}};
    expect(!search.find("mgba_libretro"), "no core anywhere is not found");
    put(root / "system" / "mgba_libretro.so", "system");
    expect(need(search.find("mgba_libretro"), "found").file == root / "system" / "mgba_libretro.so",
           "the system folder's core is found");
    put(root / "retroarch" / "mgba_libretro.so", "retroarch");
    expect(need(search.find("mgba_libretro"), "found").file ==
               root / "retroarch" / "mgba_libretro.so",
           "RetroArch's folder comes before the system's");
    put(root / "ours" / "mgba_libretro.so", "stray");
    expect(!need(search.find("mgba_libretro"), "found").downloaded,
           "a core in openSU's folder without a record is not openSU's download");
    const DownloadedCores store{root / "ours"};
    std::string error;
    expect(store.install({"mgba_libretro", "2026-10-01", 7, "u"}, "ours", "", error),
           "a record is written");
    const FoundCore ours = need(search.find("mgba_libretro"), "found");
    expect(ours.file == root / "ours" / "mgba_libretro.so" && ours.downloaded &&
               ours.downloaded->date == "2026-10-01" && read(ours.file) == "ours",
           "openSU's download comes first, with its record");
    const CoreSearch standard = CoreSearch::standard("/home/p", "/data/opensu");
    expect(standard.downloads == "/data/opensu/cores" &&
               standard.folders == std::vector<fs::path>{"/home/p/.config/retroarch/cores",
                                                         "/usr/lib64/libretro", "/usr/lib/libretro",
                                                         "/usr/lib/x86_64-linux-gnu/libretro"},
           "the standard search is openSU's, RetroArch's, then the system's folders");
    expect(standard.infoFolders ==
               std::vector<fs::path>{"/home/p/.config/retroarch/info", "/usr/share/libretro/info"},
           "info files are looked for in RetroArch's and the system's info folders");
    const CoreSearch infos{root / "mine", {}, {root / "info"}};
    put(root / "info" / "mgba_libretro.info", "theirs");
    fs::create_directories(root / "mine");
    expect(infos.info("mgba_libretro") == "theirs", "a core installed another way has its info");
    expect(DownloadedCores{root / "mine"}.installInfo("mgba_libretro", "mine", error) &&
               infos.info("mgba_libretro") == "mine",
           "openSU's info comes first");
    expect(!infos.info("snes9x_libretro"), "a core without info has none");
}

void downloadChecksAndSwitches(const fs::path& root) {
    opensu::test::BuildbotServer buildbot;
    const opensu::net::WebClient web;
    const CoreDownload download{web, buildbot.base()};
    const DownloadedCores store{root / "cores"};
    std::string error;

    const std::string info = "firmware_count = 1\nfirmware0_path = \"gba_bios.bin\"\n";
    buildbot.publish("mgba_libretro", "2026-10-01", "core one", std::nullopt, info);
    const CoreBuild first = need(download.latest("mgba_libretro", error), "the listing is read");
    const DownloadedCore installed =
        need(download.install(first, store, anyProgress, error), "a matching CRC-32 installs");
    expect(read(installed.file) == "core one" && installed.date == "2026-10-01" &&
               installed.crc32 == first.crc32 && installed.file == store.file("mgba_libretro"),
           "the core is taken out of the zip and recorded");
    expect(namesIn(store.root()) == std::vector<std::string>{"mgba_libretro.info",
                                                             "mgba_libretro.json",
                                                             "mgba_libretro.so"},
           "only the core, its info and its record stay");
    expect(read(store.info("mgba_libretro")) == info, "the core's info is the buildbot's");

    buildbot.publish("mgba_libretro", "2026-10-02", "tampered", 0x12345678);
    const CoreBuild bad = need(download.latest("mgba_libretro", error), "the update is read");
    expect(bad.date == "2026-10-02" && bad.crc32 != installed.crc32,
           "a newer build is told apart by its CRC-32");
    expect(!download.install(bad, store, anyProgress, error) &&
               error.find("CRC-32") != std::string::npos,
           "a CRC-32 mismatch is refused and named");
    expect(store.find("mgba_libretro") == installed && read(installed.file) == "core one" &&
               namesIn(store.root()).size() == 3,
           "the core before stays and nothing of the download is kept");

    buildbot.publish("mgba_libretro", "2026-10-03", "core two");
    buildbot.redirect("/nightly/mgba_libretro.so.zip",
                      buildbot.otherHost() + "mgba_libretro.so.zip");
    const CoreBuild moved = need(download.latest("mgba_libretro", error), "read");
    expect(!download.install(moved, store, anyProgress, error) &&
               error.find("not on the buildbot") != std::string::npos,
           "a redirect off the buildbot's host is refused");
    expect(store.find("mgba_libretro") == installed && namesIn(store.root()).size() == 3,
           "and nothing of it is kept");

    buildbot.redirect("/nightly/mgba_libretro.so.zip", buildbot.base() + "mgba_libretro.so.zip");
    const DownloadedCore updated = need(download.install(moved, store, anyProgress, error),
                                        "a redirect on the host is followed");
    expect(read(updated.file) == "core two" && store.find("mgba_libretro") == updated &&
               updated.date == "2026-10-03" && namesIn(store.root()).size() == 3,
           "and the verified update replaces the core");

    const CoreDownload offHost{web, buildbot.otherHost()};
    expect(!offHost.install(first, store, anyProgress, error) &&
               error.find("not on the buildbot") != std::string::npos,
           "a listing's URL off the folder's host is refused");

    expect(store.remove("mgba_libretro", error) && !store.find("mgba_libretro") &&
               namesIn(store.root()).empty(),
           "remove deletes the core, its info and its record");
}

void cancelKeepsNothing(const fs::path& root) {
    opensu::test::BuildbotServer buildbot;
    const opensu::net::WebClient web;
    const CoreDownload download{web, buildbot.base()};
    const DownloadedCores store{root / "cores"};
    buildbot.publish("mesen_libretro", "2026-10-01", std::string(std::size_t{512} * 1024, 'x'));
    std::string error;
    const CoreBuild build = need(download.latest("mesen_libretro", error), "read");
    expect(!download.install(
               build, store,
               [](std::uint64_t, std::uint64_t) {
                   return false;
               },
               error) &&
               error == "cancelled",
           "a cancelled download says so");
    expect(!store.find("mesen_libretro") && namesIn(store.root()).empty(),
           "and leaves nothing behind");
}

void foundCoreIsAnEmulator(const fs::path& root) {
    EmulatorEntry entry;
    entry.id = "gba-core";
    entry.name = "GBA (built-in)";
    entry.systems = {"gba"};
    entry.core = "mgba_libretro";
    const EmulatorCatalog catalog{{entry}};
    EmulatorSearch search;
    search.cores = CoreSearch{root / "cores", {root / "system"}, {}};
    search.runner = root / "bin" / "opensu-retro";
    search.saves = root / "saves";
    const fs::path rom = root / "Kirby.gba";

    expect(Emulators::discover(catalog, search, {}).installed("gba-core") == nullptr,
           "without its core a built-in emulator is not installed");
    const std::vector<EmulatorOption> missing =
        Emulators::discover(catalog, search, {}).options("gba", rom);
    expect(missing.size() == 1 && missing[0].name == "GBA (built-in)" && !missing[0].installed &&
               missing[0].downloadable && missing[0].id == "gba-core",
           "a ROM lists it as downloadable");

    put(root / "system" / "mgba_libretro.so", "core");
    const Emulators noRunner = Emulators::discover(catalog, search, {});
    expect(noRunner.installed("gba-core") != nullptr &&
               noRunner.installed("gba-core")->problem.find("opensu-retro") != std::string::npos &&
               !noRunner.launch("gba", rom),
           "a core without openSU's runner cannot start, and says why");

    put(search.runner, "#!/bin/sh\n");
    fs::permissions(search.runner, fs::perms::owner_all);
    const Emulators found = Emulators::discover(catalog, search, {});
    const EmulatorInstall& install = *found.installed("gba-core");
    const std::string core = (root / "system" / "mgba_libretro.so").string();
    expect(install.source == InstallSource::Core && install.where == core &&
               install.problem.empty() && !downloadedByOpensu(install.source),
           "a core in a system folder is installed, not openSU's to remove");
    const LaunchSpec spec = need(found.launch("gba", rom), "the system's games start on it");
    expect(spec.program == search.runner.string() &&
               spec.args == std::vector<std::string>{"--core", core, "--saves",
                                                     (root / "saves" / "gba").string(),
                                                     rom.string()},
           "as opensu-retro --core <core> --saves <saves>/<system> <rom>");
    const std::vector<EmulatorOption> options = found.options("gba", rom);
    expect(options.size() == 1 && options[0].installed && options[0].launch.program == spec.program,
           "and the ROM's emulator options show it installed");
    expect(found.chosenName("gba") == "GBA (built-in)", "it serves the system when alone");

    std::string error;
    expect(DownloadedCores{root / "cores"}.install({"mgba_libretro", "2026-10-05", 9, "u"}, "ours",
                                                   "", error),
           "openSU downloads one");
    const EmulatorInstall& ours = *Emulators::discover(catalog, search, {}).installed("gba-core");
    expect(ours.source == InstallSource::DownloadedCore && ours.version == "2026-10-05" &&
               label(ours.source) == "libretro buildbot" && downloadedByOpensu(ours.source),
           "openSU's download is used first, from the libretro buildbot, with its date");
}

} // namespace

int main() {
    try {
        const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "libretro_cores"};
        listingIsRead();
        lookupOrder(scratch.path() / "lookup");
        downloadChecksAndSwitches(scratch.path() / "download");
        cancelKeepsNothing(scratch.path() / "cancel");
        foundCoreIsAnEmulator(scratch.path() / "emulator");
        std::puts("libretro_cores: ok");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}
