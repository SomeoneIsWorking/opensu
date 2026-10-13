// Save RAM: the round trip, writing only what changed, the atomic replace, and a wrong-sized file.
#include "save_memory.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <sys/stat.h>

#include "check.hpp"
#include "scratch_dir.hpp"

namespace {

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

void wrongSize(const std::filesystem::path& dir) {
    const std::filesystem::path file = dir / "short.srm";
    write(file, "abc");
    std::array<std::byte, 4> ram{std::byte{9}, std::byte{9}, std::byte{9}, std::byte{9}};
    SaveMemory saves{file};
    saves.load(ram);
    expect(ram[0] == std::byte{9}, "a wrong-sized file leaves the RAM as the core made it");
    expect(saves.flush(ram), "an unchanged RAM flushes");
    expect(read(file) == "abc", "the wrong-sized file stays until the core writes");
    ram[1] = std::byte{1};
    expect(saves.flush(ram), "the core's write flushes");
    expect(read(file).size() == 4, "the core's write replaces the file");
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

int main() {
    const ScratchDir scratch{OPENSU_TEST_SCRATCH, "save_memory"};
    roundTrip(scratch.path());
    wrongSize(scratch.path());
    failedWrite(scratch.path());
    std::printf("save_memory: all checks passed\n");
    return 0;
}
