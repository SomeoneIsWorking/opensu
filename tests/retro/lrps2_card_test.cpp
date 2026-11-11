// LRPS2's per-game card through the shipping code: named after the ROM in the saves folder, started
// from a copy of the shared card when the game has none, never written over, the shared card left.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "check.hpp"
#include "lrps2_card.hpp"
#include "save_paths.hpp"
#include "scratch_dir.hpp"
#include "test_main.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::test::expect;

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void put(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << bytes;
}

} // namespace

int main() try {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "lrps2_card"};
    const fs::path system = scratch.path() / "system";
    const fs::path one = scratch.path() / "profiles" / "a" / "ps2";
    const fs::path two = scratch.path() / "profiles" / "b" / "ps2";
    const fs::path rom = "/roms/Crash Twinsanity (v1.00).chd";
    using opensu::retro::adoptLrps2SharedCard;
    using opensu::retro::lrps2CardFile;

    expect(lrps2CardFile(one, rom) == one / "Crash Twinsanity (v1.00).ps2",
           "the card is the ROM's stem with .ps2 in the saves folder");
    fs::create_directories(one);
    expect(!adoptLrps2SharedCard(system, one, rom) && !fs::exists(lrps2CardFile(one, rom)),
           "without a shared card the core makes its own");

    put(opensu::retro::lrps2SharedCard(system), "shared saves");
    expect(adoptLrps2SharedCard(system, one, rom) &&
               read(lrps2CardFile(one, rom)) == "shared saves",
           "a game without a card starts from the shared one");
    put(lrps2CardFile(one, rom), "played on");
    expect(!adoptLrps2SharedCard(system, one, rom) && read(lrps2CardFile(one, rom)) == "played on",
           "a card is never written over");
    put(lrps2CardFile(two, rom), "b's own");
    expect(!adoptLrps2SharedCard(system, two, rom) && read(lrps2CardFile(two, rom)) == "b's own",
           "another profile's card is its own");
    expect(read(opensu::retro::lrps2SharedCard(system)) == "shared saves" &&
               !fs::exists(lrps2CardFile(one, rom).string() + ".part"),
           "the shared card stays and nothing is left beside the card");
    std::puts("retro_lrps2_card: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
