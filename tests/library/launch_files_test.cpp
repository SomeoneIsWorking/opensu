// Files set for a launch and put back after it, or after a crash at the next launch.
#include "library/launch_files.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::LaunchFiles;
using opensu::library::RunFile;
using opensu::test::expect;

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void write(const fs::path& file, std::string_view bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << bytes;
}

void testSetAndPutBack() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "launch-files"};
    const fs::path had = scratch.path() / "had.ini";
    const fs::path lacked = scratch.path() / "sub" / "lacked.ini";
    write(had, "player's\n");
    {
        const LaunchFiles files{
            {RunFile{.path = had, .bytes = "run\n"}, RunFile{.path = lacked, .bytes = "made\n"}}};
        expect(read(had) == "run\n", "the run reads its file");
        expect(read(lacked) == "made\n", "a file the player lacked is made for the run");
        expect(read(scratch.path() / "had.ini.opensu-player") == "player's\n",
               "the player's file is kept beside it");
    }
    expect(read(had) == "player's\n", "the player's file comes back");
    expect(!fs::exists(lacked), "the made file goes");
    expect(!fs::exists(scratch.path() / "sub" / "lacked.ini.opensu-absent"), "no marker is left");
}

void testRecover() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "launch-files-crash"};
    const fs::path had = scratch.path() / "had.ini";
    const fs::path lacked = scratch.path() / "lacked.ini";
    write(had, "run\n");
    write(scratch.path() / "had.ini.opensu-player", "player's\n");
    write(lacked, "made\n");
    write(scratch.path() / "lacked.ini.opensu-absent", "");
    LaunchFiles::recover({had, lacked, scratch.path() / "untouched.ini"});
    expect(read(had) == "player's\n", "a crashed run's file is put back");
    expect(!fs::exists(lacked), "a crashed run's made file goes");
    expect(!fs::exists(scratch.path() / "lacked.ini.opensu-absent"), "its marker goes");
}

} // namespace

int main() try {
    testSetAndPutBack();
    testRecover();
    std::printf("launch_files: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
