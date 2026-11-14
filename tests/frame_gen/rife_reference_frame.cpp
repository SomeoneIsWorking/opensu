// rife_reference_frame MODEL_DIR A.png B.png T OUT.png PADDING: the frame at T between A and B
// made by the CPU reference of the RIFE graph, for looking at.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

#include "fileio/atomic_write.hpp"
#include "gpu2d/image/decode.hpp"
#include "gpu2d/image/png_encoder.hpp"
#include "rife_reference.hpp"

namespace {

using opensu::frame_gen::test::Blob;

Blob padded(const gpu2d::image::Bitmap& image, int padding) {
    const int w = (image.width + padding - 1) / padding * padding;
    const int h = (image.height + padding - 1) / padding * padding;
    Blob out = opensu::frame_gen::test::blob(w, h, 3);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            for (int ch = 0; ch < 3; ++ch) {
                out.at(x, y, ch) = static_cast<float>(image.rgba[image.offset(x, y) + ch]) / 255.0F;
            }
        }
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 7) {
        std::fputs("usage: rife_reference_frame MODEL_DIR A.png B.png T OUT.png PADDING\n", stderr);
        return 2;
    }
    try {
        const std::filesystem::path dir = argv[1];
        const opensu::frame_gen::ncnn::Model model =
            opensu::frame_gen::ncnn::load(dir / "flownet.param", dir / "flownet.bin");
        std::string error;
        const auto a = gpu2d::image::decodeFile(argv[2], error);
        const auto b = gpu2d::image::decodeFile(argv[3], error);
        if (!a || !b) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        const float t = std::stof(argv[4]);
        const int padding = std::stoi(argv[6]);
        Blob in0 = padded(*a, padding);
        Blob in1 = padded(*b, padding);
        Blob in2 = opensu::frame_gen::test::blob(in0.w, in0.h, 1, t);
        const std::vector<Blob> blobs =
            opensu::frame_gen::test::run(model, {{"in0", in0}, {"in1", in1}, {"in2", in2}});
        const Blob& out = blobs[static_cast<std::size_t>(model.blob("out0"))];
        gpu2d::image::Bitmap frame = *a;
        for (int y = 0; y < a->height; ++y) {
            for (int x = 0; x < a->width; ++x) {
                for (int ch = 0; ch < 3; ++ch) {
                    const float v = std::floor(out.at(x, y, ch) * 255.0F + 0.5F);
                    frame.rgba[frame.offset(x, y) + ch] =
                        static_cast<std::uint8_t>(std::clamp(v, 0.0F, 255.0F));
                }
            }
        }
        const std::string png =
            gpu2d::image::encodePng(frame.rgba, static_cast<unsigned>(frame.width),
                                    static_cast<unsigned>(frame.height), error);
        if (png.empty() || !opensu::fileio::writeWhole(argv[5], png, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
    } catch (const std::exception& failure) {
        std::fprintf(stderr, "%s\n", failure.what());
        return 1;
    }
    return 0;
}
