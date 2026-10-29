// iiSU's console art from the file the player places in the iiSU folder: the release APK (cards
// from its starter pack, glyphs from its border pack) or the starter pack alone, read from disk;
// anything else is refused with what is wrong and passed over.
#include "iisu_install.hpp"
#include "iisu_pack.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "support/test_main.hpp"
#include "support/zip_fixture.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::artwork::IisuInstall;
using opensu::artwork::IisuPack;
using opensu::test::zipfixture::buildZip;
using opensu::test::zipfixture::FixtureEntry;

constexpr std::string_view gcCard = "RIFF a GameCube card";
// The border pack maps gc to a logo in the APK, snes to one the APK lacks and n64 to one outside
// its folder; ps4 is not in it.
constexpr std::string_view borderPack =
    R"({"version": 9, "consoles": [)"
    R"({"console": "GC", "border": "gc.png", "logo": "logo_gc.png"},)"
    R"({"console": "snes", "border": "SNES.png", "logo": "logo_SNES.png"},)"
    R"({"console": "n64", "border": "n64.png", "logo": "../../classes.dex"}]})";
constexpr std::string_view gcGlyph = "\x89PNG\r\n\x1a\n glyph of the GameCube";
constexpr std::string_view navigationWav = "RIFF navigation tick";

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

fs::path write(const fs::path& file, const std::string& bytes) {
    std::ofstream{file, std::ios::binary} << bytes;
    return file;
}

std::string starterPack() {
    return buildZip({{"platforms/gc.webp", std::string{gcCard}, true, {}},
                     {"platforms/gc_title.webp", "title", false, {}}});
}

/// An APK holding the starter pack and the border pack; `fillerEntries` push the directory out of
/// the last 64 KiB.
std::string apk(int fillerEntries) {
    std::vector<FixtureEntry> entries{
        {"assets/iiSU_StarterPack.zip", starterPack(), true, {}},
        {"assets/borders/border_pack.json", std::string{borderPack}, true, {}},
        {"assets/borders/logo_gc.png", std::string{gcGlyph}, false, {}},
        {"assets/Navigation.wav", std::string{navigationWav}, true, {}},
        {"classes.dex", std::string(std::size_t{2} * 1024 * 1024, 'd'), false, {}},
    };
    for (int i = 0; i < fillerEntries; ++i) {
        entries.push_back(
            {"res/filler/" + std::string(60, 'a') + std::to_string(i), {}, false, {}});
    }
    return buildZip(entries);
}

void testRelease(const fs::path& dir, int fillerEntries) {
    std::string error;
    const auto pack = IisuPack::open(write(dir / "iiSU.apk", apk(fillerEntries)), error);
    expect(pack != nullptr && error.empty(), "iiSU's release opens");
    expect(pack->hasGlyphs() && pack->hasSounds(), "the release has glyphs and sounds");
    expect(pack->sound("Navigation.wav", error) == navigationWav, "a sound comes out of assets/");
    expect(!pack->sound("Close.wav", error) && error.empty(), "a sound it lacks is nothing");
    expect(!pack->sound("../classes.dex", error) && error.empty(),
           "a name that leaves assets/ is nothing");
    expect(pack->card("gc", error) == gcCard, "a card comes out of the nested starter pack");
    expect(!pack->card("ps4", error) && error.empty(), "a console without a card is nothing");
    expect(pack->glyph("gc", error) == gcGlyph,
           "a glyph comes from the logo the border pack names");
    expect(!pack->glyph("snes", error) && error.empty(),
           "a logo the APK lacks is nothing, not a failure");
    expect(!pack->glyph("n64", error) && !pack->glyph("ps4", error) && error.empty(),
           "a logo outside the border folder is never read");
}

void testStarterPack(const fs::path& dir) {
    std::string error;
    const auto pack = IisuPack::open(write(dir / "iiSU_StarterPack.zip", starterPack()), error);
    expect(pack != nullptr, "the starter pack alone opens");
    expect(!pack->hasGlyphs() && !pack->hasSounds(), "the starter pack has no glyphs or sounds");
    expect(!pack->sound("Navigation.wav", error) && error.empty(), "it gives no sounds");
    expect(pack->card("gc", error) == gcCard, "its cards are read straight from it");
    expect(!pack->glyph("gc", error) && error.empty(), "it gives no glyphs");
}

void testRefusals(const fs::path& dir) {
    std::string error;
    expect(IisuPack::open(dir / "absent.apk", error) == nullptr &&
               error.find("absent.apk") != std::string::npos,
           "a missing file is refused by name");
    error.clear();
    expect(IisuPack::open(write(dir / "notes.zip", "plain text, not a zip"), error) == nullptr &&
               error.find("not an APK or zip") != std::string::npos,
           "a file that is not a zip is refused");
    error.clear();
    const std::string other = buildZip({{"assets/other.png", "png", false, {}}});
    expect(IisuPack::open(write(dir / "other.apk", other), error) == nullptr &&
               error.find("not iiSU's") != std::string::npos &&
               error.find("iiSU_StarterPack.zip") != std::string::npos,
           "another app's APK is refused, naming what it lacks");
}

void testInstall(const fs::path& dir) {
    const fs::path folder = dir / "themes" / "iiSU";
    const IisuInstall install{folder};
    expect(install.pack() == nullptr, "no folder is no iiSU file");
    fs::create_directories(folder);
    write(folder / "other.apk", buildZip({{"assets/other.png", "png", false, {}}}));
    write(folder / "readme.txt", "not a pack");
    expect(install.pack() == nullptr, "another app's APK in the folder is passed over");
    write(folder / "iiSU_StarterPack.zip", starterPack());
    const auto pack = install.pack();
    expect(pack != nullptr && pack->file().filename() == "iiSU_StarterPack.zip" &&
               !pack->hasSounds(),
           "a starter pack placed gives the art");
    expect(install.pack() == pack, "the same pack while the folder is unchanged");
    write(folder / "iiSU-Alpha.APK", apk(0));
    const auto release = install.pack();
    expect(release != nullptr && release->hasSounds() &&
               release->file().filename() == "iiSU-Alpha.APK",
           "the release APK is taken before the starter pack");
    fs::remove(folder / "iiSU-Alpha.APK");
    fs::remove(folder / "iiSU_StarterPack.zip");
    expect(install.pack() == nullptr, "a file removed is no longer offered");
}

} // namespace

int main() try {
    try {
        const fs::path dir = fs::path{OPENSU_TEST_SCRATCH} / "iisu_pack";
        fs::remove_all(dir);
        fs::create_directories(dir);
        testRelease(dir, 0);
        testRelease(dir, 1200);
        testStarterPack(dir);
        testRefusals(dir);
        testInstall(dir);
        fs::remove_all(dir);
        std::printf("iisu_pack: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
} catch (...) {
    return opensu::test::escaped();
}
