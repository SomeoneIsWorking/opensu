// frame_record: one frame written per tick of the clock, the last repeated when none came.
#include "fileio/frame_record.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;
using Clock = fileio::FrameRecord::Clock;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

// One pixel, so a frame is four bytes and its first byte names it.
std::vector<std::uint8_t> pixel(std::uint8_t name) {
    return {name, 0, 0, 255};
}

std::string names(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    const std::string bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    std::string shown;
    for (std::size_t at = 0; at < bytes.size(); at += 4) {
        shown += bytes[at];
    }
    return shown;
}

void ticks() {
    const Clock::time_point start{};
    expect(fileio::FrameRecord::ticksBy(start, 60, start) == 1, "the start is the first tick");
    expect(fileio::FrameRecord::ticksBy(start, 60, start + 16ms) == 1, "within a 60th, still one");
    expect(fileio::FrameRecord::ticksBy(start, 60, start + 17ms) == 2, "past a 60th, two");
    expect(fileio::FrameRecord::ticksBy(start, 60, start + 1s) == 61, "a second on, 61");
    expect(fileio::FrameRecord::ticksBy(start + 1s, 60, start) == 0, "none before the start");
}

void paced() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "frame_record"};
    const fs::path file = scratch.path() / "frames.rgba";
    const Clock::time_point start = Clock::now();
    fileio::FrameRecord record{file, 60, {.width = 1, .height = 1}, start};
    expect(record.due(start), "the first frame is due at once");
    record.add(pixel('a'), start);
    expect(!record.due(start + 10ms), "no second frame within the first 60th");
    // 50 ms on is the fourth tick: b stands for the second, third and fourth.
    record.add(pixel('b'), start + 50ms);
    // A frame of another size repeats b for the fifth tick.
    record.add({1, 2, 3}, start + 70ms);
    // Up to 100 ms is seven ticks: b again for the sixth and seventh.
    const fileio::FrameRecord::Totals totals = record.finish(start + 100ms);
    expect(totals.error.empty(), "written without an error");
    expect(names(file) == "abbbbbb", "one frame per tick, the last repeated");
    expect(totals.frames == 7 && totals.repeated == 5, "seven frames, five of them repeats");
}

void unwritable() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "frame_record_unwritable"};
    const Clock::time_point start = Clock::now();
    fileio::FrameRecord record{
        scratch.path() / "missing" / "frames.rgba", 60, {.width = 1, .height = 1}, start};
    record.add(pixel('a'), start);
    const fileio::FrameRecord::Totals totals = record.finish(start + 20ms);
    expect(!totals.error.empty(), "a path that cannot be opened is said, not ignored");
}

} // namespace

int main() try {
    ticks();
    paced();
    unwritable();
    std::printf("frame_record: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
