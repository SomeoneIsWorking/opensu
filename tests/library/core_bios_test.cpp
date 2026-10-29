// A core's BIOS files through the shipping code: the firmware entries of a core info file, the
// file taken out of the buildbot's info archive, presence in a system folder, the mode the
// player reads, and the catalog's `withoutBios`.
#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "library/core_bios.hpp"
#include "library/emulator_catalog.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "support/zip_fixture.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu::library::roms;
using opensu::test::expect;
using opensu::test::need;

constexpr std::string_view info = R"info(# Firmware / BIOS
display_name = "Sony - PlayStation (PCSX ReARMed)"
firmware_count = 4
firmware0_desc = "scph5500.bin (PS1 JP BIOS)"
firmware0_path = "scph5500.bin"
firmware0_opt = "true"
firmware1_desc = "scph5501.bin (PS1 US BIOS)"
firmware1_path = "scph5501.bin"
firmware1_opt = "false"
firmware2_path = "../outside.bin"
firmware3_path = "bios/nested.bin"
)info";

void firmwareIsRead() {
    const std::vector<CoreFirmware> firmware = firmwareIn(info);
    expect(firmware == std::vector<CoreFirmware>{{"scph5500.bin", "PS1 JP BIOS", true},
                                                 {"scph5501.bin", "PS1 US BIOS", false},
                                                 {"bios/nested.bin", "", false}},
           "each firmware entry in order, its description without the repeated file name; a path "
           "out of the system folder is skipped");
    expect(firmwareIn("display_name = \"x\"\n").empty(), "no firmware_count lists nothing");
    expect(firmwareIn("firmware_count = \"two\"\n").empty(), "a malformed count lists nothing");
}

void archiveIsRead() {
    std::string error;
    const std::string zip = opensu::test::zipfixture::buildZip(
        {{"mgba_libretro.info", "firmware_count = 0\n", true, {}},
         {"pcsx_rearmed_libretro.info", std::string{info}, true, {}}});
    const CoreInfoArchive archive = need(CoreInfoArchive::open(zip, error), "the archive opens");
    expect(need(archive.info("pcsx_rearmed_libretro", error), "a listed info is read") == info,
           "the core's own info file");
    expect(!archive.info("snes9x_libretro", error) &&
               error.find("snes9x_libretro.info") != std::string::npos,
           "a core the archive lacks is named");
    expect(!CoreInfoArchive::open("not a zip", error), "a broken archive is refused");
}

/// A 512 KiB image `try_use_bios` takes, filed under the region `mark` (" E", " J", or none).
std::string psxImage(std::string_view mark = {}, std::size_t size = std::size_t{512} * 1024) {
    std::string image(size, '\x11');
    image.replace(1, 4, std::string{"\x00\x08\x3c\x3f", 4});
    if (!mark.empty()) {
        image.replace(0x7ff51, 2, mark);
    }
    return image;
}

void put(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << bytes;
}

void presenceAndMode(const fs::path& system) {
    const std::vector<CoreFirmware> firmware = firmwareIn(info);
    const CoreBios none = biosIn(firmware, WithoutBios::Hle, BiosScan::Listed, system);
    expect(none.files.size() == 3 && none.mode() == "HLE built in" &&
               std::ranges::none_of(none.files, &CoreBios::File::used),
           "with no file there the core's HLE is what it runs on");
    expect(biosIn(firmware, WithoutBios::Open, BiosScan::Listed, system).mode() ==
                   "Open BIOS built in" &&
               biosIn(firmware, WithoutBios::Skipped, BiosScan::Listed, system).mode() ==
                   "Not needed" &&
               biosIn(firmware, WithoutBios::Needed, BiosScan::Listed, system).mode() == "Missing",
           "each mode as the player reads it");
    put(system / "scph5501.bin", "bios");
    const CoreBios one = biosIn(firmware, WithoutBios::Hle, BiosScan::Listed, system);
    expect(!one.files[0].present && one.files[1].present && one.files[1].used &&
               one.mode() == "Your BIOS file scph5501.bin",
           "a listed file there is the one a listing core uses");
}

void pcsxRearmedScansTheFolder(const fs::path& root) {
    const std::vector<CoreFirmware> firmware = firmwareIn(info);
    const fs::path loose = root / "loose";
    put(loose / "openbios.bin", psxImage());
    put(loose / "unirom.bin", psxImage(" E"));
    put(loose / "big.bin", psxImage(" J", std::size_t{4} * 1024 * 1024));
    put(loose / "notes.txt", std::string(std::size_t{512} * 1024, 'x'));
    expect(pcsxRearmedBiosIn(loose) == std::vector<PickedBios>{{"openbios.bin", "US"}},
           "a 512 KiB image with the reset signature is taken from the folder by any name; "
           "unirom, a 4 MiB image and a file without the signature are skipped");
    const CoreBios scanned = biosIn(firmware, WithoutBios::Hle, BiosScan::PcsxRearmed, loose);
    expect(scanned.mode() == "Your BIOS file openbios.bin" && scanned.files.size() == 4 &&
               scanned.files[3].firmware.path == "openbios.bin" && scanned.files[3].used &&
               scanned.files[3].region == "US",
           "the row names the file the core will pick, after the listed ones");
    expect(biosIn(firmware, WithoutBios::Hle, BiosScan::Listed, loose).mode() == "HLE built in",
           "a core that loads only listed names ignores it");

    const fs::path named = root / "named";
    put(named / "openbios.bin", psxImage());
    put(named / "scph5501.bin", psxImage());
    std::string compatible(std::size_t{512} * 1024, '\x22');
    compatible.replace(0x12c, 13, "PS compatible");
    compatible.replace(0x7ff51, 2, " E");
    put(named / "scph7001.bin", compatible);
    put(named / "scph5500.bin", "too small");
    expect(pcsxRearmedBiosIn(named) ==
               std::vector<PickedBios>{{"scph5501.bin", "US"}, {"scph7001.bin", "EU"}},
           "the core's own names come first, one file per region; a listed name that fails "
           "the check is not taken");
    const CoreBios both = biosIn(firmware, WithoutBios::Hle, BiosScan::PcsxRearmed, named);
    expect(both.mode() == "Your BIOS files scph5501.bin, scph7001.bin" && both.files[0].present &&
               !both.files[0].used && both.files[1].region == "US",
           "a listed file the core rejects is there but not used");

    const fs::path full = root / "full";
    put(full / "scph5500.bin", psxImage(" J"));
    put(full / "scph5501.bin", psxImage());
    put(full / "scph5502.bin", psxImage(" E"));
    put(full / "zzz.bin", psxImage());
    expect(pcsxRearmedBiosIn(full).size() == 3, "with every region found by name, no scan");
}

void catalogNamesTheMode() {
    std::string error;
    const EmulatorCatalog catalog = need(EmulatorCatalog::parse(R"({"emulators": [
            {"id": "a", "name": "A", "systems": ["psx"], "core": "pcsx_rearmed_libretro",
             "withoutBios": "hle", "biosScan": "pcsx_rearmed"},
            {"id": "b", "name": "B", "systems": ["nds"], "core": "melondsds_libretro",
             "withoutBios": "open"},
            {"id": "c", "name": "C", "systems": ["gb"], "core": "gambatte_libretro"}]})",
                                                                "/home/p", error),
                                         "a catalog with modes parses");
    expect(catalog.find("a")->withoutBios == WithoutBios::Hle &&
               catalog.find("b")->withoutBios == WithoutBios::Open &&
               catalog.find("c")->withoutBios == WithoutBios::Needed,
           "each core's mode, needed when not given");
    expect(catalog.find("a")->biosScan == BiosScan::PcsxRearmed &&
               catalog.find("b")->biosScan == BiosScan::Listed,
           "a scanning core says so; the rest load listed names");
    expect(!EmulatorCatalog::parse(R"({"emulators": [{"id": "a", "name": "A", "systems": ["psx"],
               "core": "pcsx_rearmed_libretro", "withoutBios": "maybe"}]})",
                                   "/home/p", error) &&
               error.find("withoutBios") != std::string::npos,
           "an unknown mode is refused");
    expect(!EmulatorCatalog::parse(R"({"emulators": [{"id": "a", "name": "A", "systems": ["ps2"],
               "launchers": [{"programs": ["pcsx2"]}], "withoutBios": "hle"}]})",
                                   "/home/p", error),
           "a standalone emulator has no mode");
}

} // namespace

int main() try {
    try {
        const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "core_bios"};
        firmwareIsRead();
        archiveIsRead();
        presenceAndMode(scratch.path() / "system");
        pcsxRearmedScansTheFolder(scratch.path() / "scan");
        catalogNamesTheMode();
        std::puts("core_bios: ok");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
} catch (...) {
    return opensu::test::escaped();
}
