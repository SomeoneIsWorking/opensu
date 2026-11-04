// Save RAM: the round trip, writing only what changed, the atomic replace, a smaller file loaded
// into the start, another core's layout fitted (MBC2's packing, blank bytes past the RAM), and a
// larger or unreadable file refused and left as it is.
#include "save_memory.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>

#include <sys/stat.h>

#include "check.hpp"
#include "scratch_dir.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::retro::SaveError;
using opensu::retro::SaveMemory;
using opensu::test::expect;
using opensu::test::ScratchDir;

std::string read(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void write(const std::filesystem::path& file, const std::string& bytes) {
    std::ofstream out{file, std::ios::binary};
    out << bytes;
}

void roundTrip(const std::filesystem::path& dir) {
    const std::filesystem::path file = dir / "game" / "game.srm";
    std::array<std::byte, 4> ram{};
    SaveMemory saves{file};
    saves.load(ram);
    expect(saves.flush(ram), "an unchanged RAM flushes");
    expect(!std::filesystem::exists(file), "an unchanged RAM writes nothing");
    ram[0] = std::byte{0x12};
    ram[3] = std::byte{0x34};
    expect(saves.flush(ram), "a changed RAM flushes");
    expect(read(file) == std::string{"\x12\0\0\x34", 4}, "the file holds the RAM");
    expect(!std::filesystem::exists(dir / "game" / "game.srm.part"), "no partial file is left");

    std::array<std::byte, 4> again{};
    SaveMemory reloaded{file};
    reloaded.load(again);
    expect(again == ram, "a load restores the RAM");
}

void smallerFile(const std::filesystem::path& dir) {
    const std::filesystem::path file = dir / "short.srm";
    write(file, "ab");
    std::array<std::byte, 4> ram{std::byte{9}, std::byte{9}, std::byte{9}, std::byte{9}};
    SaveMemory saves{file};
    saves.load(ram);
    expect(ram[0] == std::byte{'a'} && ram[1] == std::byte{'b'},
           "a smaller file fills the start of the RAM");
    expect(ram[2] == std::byte{9}, "the rest stays as the core made it");
    expect(saves.flush(ram), "an unchanged RAM flushes");
    expect(read(file) == "ab", "an unchanged RAM leaves the smaller file");
    expect(saves.flush(std::span{ram}.first(2)), "RAM shrunk to its loaded start flushes");
    expect(read(file) == "ab", "RAM shrunk to its loaded start is unchanged");
    ram[1] = std::byte{'c'};
    expect(saves.flush(std::span{ram}.first(2)), "the core's write flushes");
    expect(read(file) == "ac", "the core's write replaces the file at the RAM's size");
}

void largerFile(const std::filesystem::path& dir) {
    const std::filesystem::path file = dir / "long.srm";
    write(file, "abcdef");
    std::array<std::byte, 4> ram{};
    SaveMemory saves{file};
    bool refused = false;
    try {
        saves.load(ram);
    } catch (const SaveError&) {
        refused = true;
    }
    expect(refused, "a file larger than the RAM is refused");
    expect(read(file) == "abcdef", "the refused file is left as it is");
}

void anotherCoresLayout(const std::filesystem::path& dir) {
    // gambatte keeps an MBC2 cartridge's 512 cells a byte each in an 8 KiB bank; mGBA packs them.
    const std::filesystem::path file = dir / "mbc2.srm";
    std::string cells(0x2000, '\xFF');
    cells[0] = '\xF1';
    cells[1] = '\xF2';
    cells[0x1FF] = '\xF9';
    write(file, cells);
    std::array<std::byte, 0x100> packed{};
    SaveMemory{file}.load(packed);
    expect(packed[0] == std::byte{0x21} && packed[0xFF] == std::byte{0x9F},
           "gambatte's cells are packed two to a byte, the even cell low, for mGBA");
    write(file, std::string(reinterpret_cast<const char*>(packed.data()), packed.size()));
    std::array<std::byte, 0x2000> unpacked{};
    SaveMemory{file}.load(unpacked);
    expect(unpacked[0] == std::byte{0xF1} && unpacked[1] == std::byte{0xF2} &&
               unpacked[0x1FF] == std::byte{0xF9} && unpacked[0x200] == std::byte{0xF1},
           "and mGBA's unpacked a cell a byte for gambatte, mirrored through the bank");

    const std::filesystem::path padded = dir / "padded.srm";
    write(padded, "ab" + std::string(6, '\xFF'));
    std::array<std::byte, 4> ram{};
    SaveMemory{padded}.load(ram);
    expect(ram[0] == std::byte{'a'} && ram[2] == std::byte{0xFF},
           "a longer file whose bytes past the RAM hold nothing loads its start");
    expect(read(padded).size() == 8, "and is left as it is until the game writes");
}

void unreadableFile(const std::filesystem::path& dir) {
    const std::filesystem::path file = dir / "unreadable.srm";
    write(file, "ab");
    chmod(file.c_str(), 0200);
    std::array<std::byte, 4> ram{};
    SaveMemory saves{file};
    bool refused = false;
    try {
        saves.load(ram);
    } catch (const SaveError&) {
        refused = true;
    }
    chmod(file.c_str(), 0600);
    expect(refused, "a file that cannot be read is refused");
    expect(read(file) == "ab", "the unreadable file is left as it is");
}

void failedWrite(const std::filesystem::path& dir) {
    const std::filesystem::path locked = dir / "locked";
    std::filesystem::create_directories(locked);
    const std::filesystem::path file = locked / "kept.srm";
    write(file, std::string(4, 'k'));
    std::array<std::byte, 4> ram{};
    SaveMemory saves{file};
    saves.load(ram);
    ram[0] = std::byte{1};
    chmod(locked.c_str(), 0500);
    const bool flushed = saves.flush(ram);
    chmod(locked.c_str(), 0700);
    expect(!flushed, "a write that cannot happen reports it");
    expect(read(file) == std::string(4, 'k'), "the previous save survives a failed write");
    expect(saves.flush(ram), "the next flush tries again");
    expect(read(file) == "\x01kkk", "and writes the RAM");
}

} // namespace

int main() try {
    const ScratchDir scratch{OPENSU_TEST_SCRATCH, "save_memory"};
    roundTrip(scratch.path());
    smallerFile(scratch.path());
    largerFile(scratch.path());
    anotherCoresLayout(scratch.path());
    unreadableFile(scratch.path());
    failedWrite(scratch.path());
    std::printf("save_memory: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
