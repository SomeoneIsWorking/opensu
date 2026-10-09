// The image decoder: decoding runs on its own thread, big covers are scaled down, the frame takes
// a bounded number of results at a time, and queued work for tiles that left the window is dropped.
#include "image_decoder.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <thread>

#include "check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::test::expect;
using opensu::ui::DecodeJob;
using opensu::ui::ImageDecoder;

fs::path writeCover(const fs::path& dir, const char* name, int width, int height) {
    const fs::path file = dir / name;
    Image image = GenImageColor(width, height, Color{200, 40, 40, 255});
    ExportImage(image, file.string().c_str());
    UnloadImage(image);
    return file;
}

DecodeJob job(std::size_t tile, const fs::path& file) {
    return DecodeJob{.shelf = 1, .tile = tile, .ticket = 7, .portrait = file, .wide = {}};
}

void decodesOffThreadAndScalesDown(const fs::path& dir) {
    const fs::path big = writeCover(dir, "big.png", 1200, 1600);
    const fs::path small = writeCover(dir, "small.png", 100, 150);
    ImageDecoder decoder;
    decoder.request(job(0, big));
    decoder.request(job(1, small));
    decoder.request(job(2, dir / "missing.png"));
    decoder.waitDecoded();
    std::vector<opensu::ui::Decoded> done = decoder.take(10);
    expect(done.size() == 3, "every job finishes");
    expect(done[0].job.tile == 0 && done[0].job.ticket == 7, "a result carries its job");
    expect(done[0].portrait.image().height == opensu::ui::maxCoverSide &&
               done[0].portrait.image().width == opensu::ui::maxCoverSide * 3 / 4,
           "a big cover is scaled down to the texture limit, keeping its aspect");
    expect(done[1].portrait.image().width == 100, "a small cover is left as it is");
    expect(done[2].portrait.empty(), "a missing file gives no pixels");
    expect(decoder.idle(), "nothing is left");
}

void handsOverABoundedNumberAtATime(const fs::path& dir) {
    const fs::path cover = writeCover(dir, "cover.png", 50, 50);
    ImageDecoder decoder;
    for (std::size_t tile = 0; tile < 10; ++tile) {
        decoder.request(job(tile, cover));
    }
    decoder.waitDecoded();
    expect(decoder.take(4).size() == 4, "the first frame takes its budget");
    expect(decoder.take(4).size() == 4, "the second takes the next");
    expect(decoder.take(4).size() == 2, "the third takes the rest");
    expect(decoder.take(4).empty(), "then nothing");
}

void dropsQueuedWorkOutsideTheWindow(const fs::path& dir) {
    const fs::path cover = writeCover(dir, "queued.png", 1200, 1600);
    ImageDecoder decoder;
    for (std::size_t tile = 0; tile < 40; ++tile) {
        decoder.request(job(tile, cover));
    }
    const std::vector<DecodeJob> dropped = decoder.prune([](const DecodeJob& queued) {
        return queued.tile < 2;
    });
    expect(dropped.size() >= 30, "queued jobs outside the window are returned, not decoded");
    decoder.waitDecoded();
    expect(decoder.take(100).size() + dropped.size() == 40, "no job is lost or done twice");
}

} // namespace

int main() {
    const fs::path dir = fs::path{OPENSU_TEST_SCRATCH} / "image-decoder";
    fs::remove_all(dir);
    fs::create_directories(dir);
    SetTraceLogLevel(LOG_ERROR);
    decodesOffThreadAndScalesDown(dir);
    handsOverABoundedNumberAtATime(dir);
    dropsQueuedWorkOutsideTheWindow(dir);
    fs::remove_all(dir);
    std::printf("image_decoder: all checks passed\n");
    return 0;
}
