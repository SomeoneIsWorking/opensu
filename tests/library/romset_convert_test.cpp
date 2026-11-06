// An older romset rebuilt for a core's listing, on made-up bytes: a whole file, a mirrored chip,
// byte-interleaved files, a key from its words, an old zip name, what a wrong recipe does, the
// kept zip, and the verdict picking a core that converts.
#include "library/romset_convert.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "library/romset_match.hpp"
#include "zip/zip_archive.hpp"
#include "zip/zip_writer.hpp"

namespace {

namespace fs = std::filesystem;
namespace zip = opensu::zip;
using namespace opensu::library;
using namespace opensu::library::roms;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

/// What `value` holds, or the test ends.
template <typename Optional> auto& present(Optional& value, const char* what) {
    if (!value.has_value()) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    return *value;
}

inline constexpr std::string_view whole = "the whole file";
inline constexpr std::string_view chip = "one chip's bytes";
inline constexpr std::string_view mirrored = "one chip's bytesone chip's bytes";
inline constexpr std::string_view laneA = "abcdefgh";
inline constexpr std::string_view laneB = "ABCDEFGH";
const Cps2Key gameKey{1, 2, 3, 4, 5, 6, 7, 0x4000, 0x0900, 0x03c0};

std::string interleaved() {
    std::string out;
    for (std::size_t i = 0; i < laneA.size(); ++i) {
        out += laneA[i];
        out += laneB[i];
    }
    return out;
}

std::string datafile() {
    const auto row = [](std::string_view name, std::string_view bytes) {
        return std::format(R"(<rom name="{}" size="{}" crc="{:08x}"/>)", name, bytes.size(),
                           zip::crc32Of(bytes));
    };
    return "<datafile><game name=\"real\">" + row("whole.1", whole) + row("chip.1", chip) +
           row("lane.a", laneA) + row("lane.b", laneB) + row("real.key", cps2KeyFile(gameKey)) +
           "</game></datafile>";
}

std::string conversions(std::uint32_t laneBStart) {
    const std::string source = std::to_string(zip::crc32Of(interleaved()));
    return std::format(
        R"({{"recipes":[
        {{"crc":{},"pieces":[[{},0,1,{},0]]}},
        {{"crc":{},"pieces":[[{},0,2,{},0]]}},
        {{"crc":{},"pieces":[[{},{},2,{},0]]}}],
        "aliases":{{"oldname":["real"]}},"optional":[]}})",
        zip::crc32Of(chip), zip::crc32Of(mirrored), chip.size(), zip::crc32Of(laneA), source,
        laneA.size(), zip::crc32Of(laneB), source, laneBStart, laneB.size());
}

std::string keys() {
    std::string words;
    for (const std::uint16_t word : gameKey) {
        words += (words.empty() ? "" : ",") + std::to_string(word);
    }
    return std::format(R"({{"keys":[{{"crc":{},"words":[{}]}}]}})",
                       zip::crc32Of(cps2KeyFile(gameKey)), words);
}

void writeOldZip(const fs::path& file) {
    fs::create_directories(file.parent_path());
    std::ofstream out{file, std::ios::binary | std::ios::trunc};
    zip::Writer writer{out};
    std::string error;
    check(writer.add("old.1", whole, error) && writer.add("old.2", mirrored, error) &&
              writer.add("old.3", interleaved(), error) && writer.finish(error),
          "the old zip is written");
}

RomsetConversions conversionsWith(std::uint32_t laneBStart) {
    RomsetConversions data;
    std::string error;
    check(data.addConversions(conversions(laneBStart), error), "the recipes parse");
    check(data.addKeys(keys(), error), "the keys parse");
    return data;
}

} // namespace

int main() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "romset-convert";
    std::error_code ec;
    fs::remove_all(root, ec);
    const fs::path rom = root / "ROM" / "oldname.zip";
    writeOldZip(rom);

    check(zip::crc32Of(cps2KeyFile(Cps2Key{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
                                           0xffff, 0xffff, 0xffff})) == 0x2cf772b0,
          "a dead board's key is 20 bytes of 0xff");

    RomsetDat dat;
    dat.addXml(datafile());
    EmulatorEntry entry;
    entry.id = "core";
    entry.name = "Core";
    const RomsetDats dats = RomsetDats::under(root / "cache", nullptr);
    std::string error;
    check(dats.save(entry, dat, 1, RomsetDats::Clock::now(), error), "the listing is kept");

    const std::shared_ptr<const RomTree> tree = openRomTree();
    const std::vector<RomNode> nodes =
        tree->children(rom.parent_path()).value_or(std::vector<RomNode>{});
    const RomFolder folder{*tree, rom.parent_path(), nodes};
    FolderZips zips{folder};

    check(matchSet("oldname", dat, zips).fit == RomsetFit::Mismatch, "no plain match");
    const std::optional<RomsetVerdict> verdict =
        convertSet("oldname", dat, conversionsWith(1), zips);
    check(verdict && verdict->fit == RomsetFit::Converts && verdict->target == "real",
          "the old name converts to its set");

    RomsetDat lacking;
    lacking.addXml(
        R"(<datafile><game name="real"><rom name="x" size="3" crc="01020304"/></game></datafile>)");
    check(!convertSet("oldname", lacking, conversionsWith(1), zips),
          "a ROM nothing supplies refuses the conversion");

    const RomConversion conversion{"real", "core", {rom}, "stamp"};
    const RomsetConverter converter{root / "cache", tree, conversionsWith(1), dats};
    const std::optional<fs::path> kept = converter.build(conversion, entry, error);
    const fs::path& built = present(kept, error.c_str());
    check(built.filename() == "real.zip", "the zip takes the set's name");

    std::ifstream in{built, std::ios::binary};
    std::string bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    std::optional<zip::Archive> opened = zip::Archive::open(std::move(bytes), error);
    zip::Archive& archive = present(opened, "the rebuilt zip opens");
    check(archive.entries().size() == 5, "every ROM is in it");
    check(archive.read("whole.1", error) == whole, "a held file is copied");
    check(archive.read("chip.1", error) == chip, "a mirrored chip is halved");
    check(archive.read("lane.a", error) == laneA && archive.read("lane.b", error) == laneB,
          "interleaved files are parted");
    check(archive.read("real.key", error) == cps2KeyFile(gameKey), "the key is written");

    const fs::file_time_type made = fs::last_write_time(built);
    check(converter.build(conversion, entry, error) == kept, "a built zip is kept");
    check(fs::last_write_time(built) >= made, "a kept zip counts as used");

    const RomConversion other{"real", "core", {rom}, "changed"};
    const RomsetConverter wrong{root / "cache", tree, conversionsWith(0), dats};
    check(!wrong.build(other, entry, error), "bytes unlike the listing refuse the zip");
    check(!error.empty(), "the refusal says why");

    Game game;
    game.emulator = "MAME 2010";
    EmulatorOption first;
    first.name = "FinalBurn Neo";
    first.id = "core";
    first.installed = true;
    first.launch.program = "runner";
    EmulatorOption second;
    second.name = "MAME 2010";
    second.id = "mame";
    second.installed = true;
    game.emulatorOptions = {first, second};
    RomsetVerdict converts = present(verdict, "the set converts");
    converts.emulatorId = "core";
    RomsetVerdict mismatch;
    mismatch.emulatorId = "mame";
    mismatch.fit = RomsetFit::Mismatch;
    addConversions(game, folder, nodes.front(), {converts, mismatch});
    applyRomsets(game, "oldname", {converts, mismatch});
    check(game.emulator == "FinalBurn Neo", "the converting core is picked");
    check(game.launch.conversion && game.launch.conversion->target == "real",
          "the launch carries the conversion");
    check(game.romset == "Matches FinalBurn Neo through conversion", "the verdict says so");
    return 0;
}
