// The emulator catalog: the shipped file parses into entries the search can use, and a malformed
// file is refused with where it is wrong.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "library/emulator_catalog.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu::library::roms;
using opensu::test::expect;

void shippedCatalogParses() {
    const EmulatorCatalog catalog = EmulatorCatalog::shipped(OPENSU_ASSETS_DIR, "/home/p");
    expect(catalog.entries().size() >= 10, "the shipped catalog lists the emulators");
    const EmulatorEntry* found = catalog.find("pcsx2");
    expect(found != nullptr, "PCSX2 is listed");
    const EmulatorEntry& pcsx2 = *found;
    expect(pcsx2.name == "PCSX2" && pcsx2.serves("ps2") && pcsx2.release &&
               pcsx2.release->downloads == "https://github.com/PCSX2/pcsx2/releases/download/",
           "with its systems and its official release host");
    expect(pcsx2.config == "/home/p/.config/PCSX2", "~ in a config path is the home folder");
    const EmulatorEntry* dolphin = catalog.findByName("Dolphin");
    expect(dolphin != nullptr && !dolphin->release &&
               dolphin->launchers.front().flatpak == "org.DolphinEmu.dolphin-emu",
           "an emulator without an official AppImage is found, never downloaded");
    const EmulatorEntry* shad = catalog.find("shadps4");
    expect(shad != nullptr && shad->launchers.size() == 2, "an emulator may start two ways");
    expect(shad->release && shad->release->asset.ends_with("\\.zip$") &&
               !shad->release->member.empty(),
           "a release that ships its AppImage in a zip names it");
    for (const EmulatorEntry& entry : catalog.entries()) {
        expect((entry.launchers.empty() == entry.builtIn()) && !entry.systems.empty(),
               "every entry is usable: launchers, or a core built in");
        expect(!entry.release || entry.release->downloads.starts_with("https://"),
               "every download is HTTPS from the project's own host");
    }
    const EmulatorEntry* mgba = catalog.find("mgba-core");
    expect(mgba != nullptr && mgba->name == "mGBA (built-in)" && mgba->core == "mgba_libretro" &&
               mgba->serves("gb") && mgba->serves("gbc") && mgba->serves("gba") &&
               mgba->launchers.empty() && !mgba->release && mgba->downloadable(),
           "a built-in emulator names its core and is downloadable");
    const EmulatorEntry* link = catalog.find("mgba-link-core");
    expect(link != nullptr && link->core == "mgba_link_libretro" && link->shipped &&
               link->serves("gba") && !link->serves("gb") && !link->downloadable() &&
               !mgba->shipped,
           "the mGBA link core ships with openSU, for GBA, and is never downloaded");
    expect(catalog.find("mgba") != nullptr && !catalog.find("mgba")->builtIn(),
           "the standalone mGBA stays beside it");
    for (const char* id :
         {"fbneo-core", "fbalpha2012-core", "mame2003-plus-core", "mame2010-core", "mame-core"}) {
        const EmulatorEntry* arcade = catalog.find(id);
        expect(arcade != nullptr && arcade->builtIn() && arcade->serves("arcade") &&
                   !arcade->romsets.empty(),
               "each arcade core is built in and names its romset listings");
    }
    const std::optional<ReleaseSource>& mameListing = catalog.find("mame-core")->romsets.release;
    expect(catalog.find("fbneo-core")->romsets.files.size() == 2 &&
               mameListing.value_or(ReleaseSource{}).downloads ==
                   "https://github.com/mamedev/mame/releases/download/",
           "FinalBurn Neo's arcade and Neo Geo listings, MAME's from its release");
    expect(catalog.find("mgba-core")->romsets.empty(), "a console core has no romset listing");
}

void coreReleasePerBuild() {
    constexpr std::string_view catalog =
        R"({"emulators":[{"id":"a","name":"A","systems":["arcade"],"core":"fbneo_libretro",
            "release":{"api":"https://h/api","downloads":"https://h/dl/",
                       "assets":{"android-arm64-v8a":"^a-arm64\\.zip$"}}}]})";
    std::string error;
    const std::optional<EmulatorCatalog> named =
        EmulatorCatalog::parse(catalog, "/h", error, "android-arm64-v8a");
    expect(named && named->find("a")->release.value_or(ReleaseSource{}).asset == "^a-arm64\\.zip$",
           "a build the core's release names takes its asset");
    const std::optional<EmulatorCatalog> other =
        EmulatorCatalog::parse(catalog, "/h", error, "linux-x86_64");
    expect(other && other->find("a") != nullptr && !other->find("a")->release,
           "every other build takes the buildbot's core");
    expect(!EmulatorCatalog::parse(
               R"({"emulators":[{"id":"a","name":"A","systems":["arcade"],
                   "core":"fbneo_libretro","release":{"api":"https://h/api",
                   "downloads":"https://h/dl/","asset":"x"}}]})",
               "/h", error) &&
               error.find("by build") != std::string::npos,
           "a core's release without assets by build is refused");

    std::ifstream shipped{std::filesystem::path{OPENSU_ASSETS_DIR} / "emulators.json",
                          std::ios::binary};
    const std::string body{std::istreambuf_iterator<char>{shipped},
                           std::istreambuf_iterator<char>{}};
    for (const std::string_view build : {"android-arm64-v8a", "android-x86_64"}) {
        const std::optional<EmulatorCatalog> android =
            EmulatorCatalog::parse(body, "/h", error, build);
        expect(android && android->find("mame2010-core") != nullptr &&
                   android->find("mame2010-core")->release.has_value(),
               "MAME 2010 comes from openSU's own build on Android");
    }
}

void romsetSources() {
    std::string error;
    const std::optional<EmulatorCatalog> parsed = EmulatorCatalog::parse(
        R"({"emulators":[{"id":"a","name":"A","systems":["arcade"],"core":"fbneo_libretro",
            "romsets":{"files":["https://example.org/a.dat"]}}]})",
        "/h", error);
    expect(parsed && parsed->find("a")->romsets.files.front() == "https://example.org/a.dat",
           "a core's romset listings parse");
    expect(!EmulatorCatalog::parse(
               R"({"emulators":[{"id":"a","name":"A","systems":["arcade"],"core":"fbneo_libretro",
                   "romsets":{"files":["http://example.org/a.dat"]}}]})",
               "/h", error) &&
               error.find("HTTPS") != std::string::npos,
           "a listing that is not fetched over HTTPS is refused");
    expect(!EmulatorCatalog::parse(
               R"({"emulators":[{"id":"a","name":"A","systems":["arcade"],"core":"fbneo_libretro",
                   "romsets":{}}]})",
               "/h", error) &&
               error.starts_with("a:"),
           "empty romsets are refused, naming the entry");
}

void coreEntries() {
    std::string error;
    const std::optional<EmulatorCatalog> parsed = EmulatorCatalog::parse(
        R"({"emulators":[{"id":"snes-core","name":"S","systems":["snes"],"core":"snes9x_libretro"}]})",
        "/h", error);
    expect(parsed && parsed->find("snes-core")->core == "snes9x_libretro",
           "a core entry parses without launchers");
    expect(!EmulatorCatalog::parse(
               R"({"emulators":[{"id":"bad","name":"B","systems":["gb"],"core":"../evil"}]})", "/h",
               error) &&
               error.starts_with("bad:") && error.find("core") != std::string::npos,
           "a core that is not a core's name is refused, naming the entry");
    expect(!EmulatorCatalog::parse(R"({"emulators":[{"id":"both","name":"B","systems":["gb"],
              "core":"mgba_libretro","launchers":[{}]}]})",
                                   "/h", error) &&
               error.starts_with("both:") && error.find("launchers") != std::string::npos,
           "a core entry with launchers is refused, naming the entry");
    expect(!EmulatorCatalog::parse(R"({"emulators":[{"id":"none","name":"N","systems":["gb"]}]})",
                                   "/h", error) &&
               error == "none: no launchers and no core",
           "an entry with neither is refused");
}

void malformedIsRefused() {
    std::string error;
    expect(!EmulatorCatalog::parse("[]", "/h", error), "not an object");
    expect(!EmulatorCatalog::parse(R"({"emulators":[{"name":"X","systems":["gb"],
              "launchers":[{"args":[]}]}]})",
                                   "/h", error) &&
               error.find("\"id\"") != std::string::npos,
           "a missing id is named");
    expect(!EmulatorCatalog::parse(R"({"emulators":[{"id":"x","name":"X","systems":["gb"],
              "launchers":[{}],"release":{"api":"a","asset":"(","downloads":"https://h/"}}]})",
                                   "/h", error) &&
               error.find("regular expression") != std::string::npos,
           "a bad asset pattern is named");
    expect(!EmulatorCatalog::parse(R"({"emulators":[{"id":"x","name":"X","systems":["gb"],
              "launchers":[{}],"release":{"api":"a","asset":"x","member":"[",
              "downloads":"https://h/"}}]})",
                                   "/h", error) &&
               error.find("\"member\" is not a regular expression") != std::string::npos,
           "a bad member pattern is named");
    expect(!EmulatorCatalog::parse(R"({"emulators":[{"id":"x","name":"X","systems":["gb"],
              "launchers":[{}],"release":{"api":"a","asset":"x","downloads":"https://h"}}]})",
                                   "/h", error) &&
               error.find("downloads") != std::string::npos,
           "a download host without its path is refused");
    expect(!EmulatorCatalog::parse(R"({"emulators":[
              {"id":"x","name":"X","systems":["gb"],"launchers":[{}]},
              {"id":"x","name":"Y","systems":["gb"],"launchers":[{}]}]})",
                                   "/h", error) &&
               error.find("twice") != std::string::npos,
           "an id listed twice is refused");
}

} // namespace

int main() try {
    shippedCatalogParses();
    coreReleasePerBuild();
    malformedIsRefused();
    coreEntries();
    romsetSources();
    std::puts("emulator_catalog: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
