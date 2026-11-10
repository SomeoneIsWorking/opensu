// The visual preset catalog: the shipped file parses, Auto Console finds a console's own preset,
// a preset applies its shader (or its filter without the pack), aspect and the art that is
// downloaded, and a malformed catalog is refused.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "library/visual_presets.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu::library::roms;
using opensu::settings::Aspect;
using opensu::settings::Filter;
using opensu::settings::VideoSettings;
using opensu::test::expect;
using opensu::test::need;

constexpr std::string_view small = R"({
  "art": [{"id": "tv", "url": "https://art.example/tv.png", "size": 4,
           "sha256": "0000000000000000000000000000000000000000000000000000000000000000"}],
  "presets": [
    {"id": "crt", "shader": "crt/crt-lottes.slangp", "filter": "crt", "aspect": "4:3",
     "landscape": {"art": "tv", "picture": [10, 20, 30, 40]}},
    {"id": "nes", "consoles": ["nes"], "portrait": {"art": "tv", "picture": [1, 2, 3, 4]}}
  ]})";

void shippedParses() {
    const VisualPresets shipped = VisualPresets::shipped(OPENSU_ASSETS_DIR, "/data");
    expect(shipped.find("crt") != nullptr, "CRT is in the shipped catalog");
    expect(shipped.forConsole("nes") != nullptr, "the NES has a preset of its own");
    expect(shipped.chosen("auto-console", "nes") == shipped.forConsole("nes") &&
               shipped.chosen("auto-console", "psx") == nullptr &&
               shipped.chosen("none", "nes") == nullptr,
           "Auto Console picks the console's own, a console without one runs as None");
    for (const PresetArt& art : shipped.art()) {
        expect(art.url.starts_with("https://raw.githubusercontent.com/libretro/common-overlays/"),
               "every image comes from libretro common-overlays, pinned");
        expect(shipped.fileOf(art) == fs::path{"/data/overlays"} / (art.id + ".png"),
               "and is kept under the data folder's overlays");
    }
}

void appliesWhatIsHere() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "visual-presets";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "overlays");
    fs::create_directories(root / "shaders" / "crt");
    std::string error;
    const VisualPresets presets =
        need(VisualPresets::parse(small, root / "overlays", error), "a small catalog parses");
    const VisualPreset& crt = *presets.find("crt");

    VideoSettings video;
    expect(!presets.apply(crt, video, root / "shaders"), "art not downloaded is reported");
    expect(video.shaderPreset.empty() && video.filter == Filter::Crt &&
               video.aspect == Aspect::FourThree && !video.landscapeArt,
           "without the pack its filter stands in, and no frame without the art");

    std::ofstream{root / "shaders" / "crt" / "crt-lottes.slangp"} << "x";
    std::ofstream{root / "overlays" / "tv.png", std::ios::binary} << "abcd";
    video = {};
    expect(presets.apply(crt, video, root / "shaders"), "with its art it is whole");
    expect(video.shaderPreset == root / "shaders" / "crt" / "crt-lottes.slangp",
           "its shader from the pack");
    expect(video.landscapeArt && video.landscapeArt->image == root / "overlays" / "tv.png" &&
               video.landscapeArt->x == 10 && video.landscapeArt->height == 40 &&
               !video.portraitArt,
           "its landscape frame with the picture rect");
    expect(presets.missing().empty() && presets.downloadedCount() == 1, "the art counts as here");
    expect(presets.remove(error) && !presets.missing().empty(), "and is removed");
}

void malformedIsRefused() {
    std::string error;
    expect(!VisualPresets::parse(R"({"art": [], "presets": [{"id": "crt",
               "landscape": {"art": "tv", "picture": [0, 0, 1, 1]}}]})",
                                 "/o", error) &&
               error.find("not listed") != std::string::npos,
           "art that is not listed is refused");
    expect(!VisualPresets::parse(R"({"art": [{"id": "tv", "url": "http://a/b.png", "size": 1,
               "sha256": "0000000000000000000000000000000000000000000000000000000000000000"}],
               "presets": []})",
                                 "/o", error) &&
               error.find("HTTPS") != std::string::npos,
           "art over plain HTTP is refused");
    expect(!VisualPresets::parse(R"({"art": [], "presets": [{"id": "x", "aspect": "wide"}]})", "/o",
                                 error),
           "an aspect that is no choice is refused");
}

} // namespace

int main() try {
    shippedParses();
    appliesWhatIsHere();
    malformedIsRefused();
    std::puts("visual_presets: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
