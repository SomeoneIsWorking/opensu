#include "frame_png.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unistd.h>
#include <vector>

#include "lucent/log.h"
#include "raylib.h"

namespace opensu::app {
namespace {

/// Flips an image in place, for the render texture's bottom-up origin.
void flipVertical(Image& image) {
    const int stride = image.width * 4;
    std::vector<unsigned char> row(static_cast<std::size_t>(stride));
    auto* pixels = static_cast<unsigned char*>(image.data);
    for (int y = 0; y < image.height / 2; ++y) {
        unsigned char* top = pixels + static_cast<std::size_t>(y) * stride;
        unsigned char* bottom = pixels + static_cast<std::size_t>(image.height - 1 - y) * stride;
        std::memcpy(row.data(), top, static_cast<std::size_t>(stride));
        std::memcpy(top, bottom, static_cast<std::size_t>(stride));
        std::memcpy(bottom, row.data(), static_cast<std::size_t>(stride));
    }
}

} // namespace

bool renderShellPng(ui::Shell& shell, std::string& png) {
    const RenderTexture target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    if (target.id == 0) {
        lucent::error("render", "could not create an offscreen target");
        return false;
    }

    shell.draw(&target);

    // A render texture has OpenGL's bottom-up origin, so the exported image is
    // the frame upside down. Flipping the rows here keeps draw() identical
    // between the window and this path.
    Image frame = LoadImageFromTexture(target.texture);
    flipVertical(frame);

    // raylib can only encode to a file, so the bytes are written there and read
    // back. The name carries the process id, so two shells on one machine do
    // not fight over it.
    const std::string path = (std::filesystem::temp_directory_path() /
                              ("opensu-frame-" + std::to_string(::getpid()) + ".png"))
                                 .string();
    const bool written = ExportImage(frame, path.c_str());
    UnloadImage(frame);
    UnloadTexture(target.texture);
    if (!written) {
        lucent::error("render", "could not encode the frame");
        return false;
    }

    std::ifstream in{path, std::ios::binary};
    png.assign(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});
    in.close();
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return !png.empty();
}

} // namespace opensu::app
