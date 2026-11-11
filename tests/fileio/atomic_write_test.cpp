// atomic_write: a file replaced whole through a part beside it, or left as it was.
#include "fileio/atomic_write.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;
namespace fs = std::filesystem;

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void madeAndReplaced() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "atomic_write"};
    const fs::path file = scratch.path() / "deep" / "settings.ini";
    std::string error;
    expect(fileio::writeWhole(file, "first\n", error) && read(file) == "first\n",
           "a missing file is made with its folders");
    expect(fileio::writeWhole(file, "x", error) && read(file) == "x",
           "an existing file is replaced whole, not overwritten in front");
    expect(!fs::exists(scratch.path() / "deep" / "settings.ini.part"), "no part is left");
}

void leftAsItWas() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "atomic_write_fail"};
    const fs::path folder = scratch.path() / "taken";
    fs::create_directories(folder / "inside");
    std::string error;
    expect(!fileio::writeWhole(folder, "bytes", error) && !error.empty(),
           "a path a folder holds is refused with a reason");
    expect(fs::is_directory(folder / "inside"), "the folder is as it was");
    expect(!fs::exists(scratch.path() / "taken.part"), "and no part is left beside it");
}

} // namespace

int main() try {
    madeAndReplaced();
    leftAsItWas();
    std::printf("atomic_write: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
