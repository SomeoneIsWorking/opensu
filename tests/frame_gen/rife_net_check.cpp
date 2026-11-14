// Command rife_net_check runs RIFE's graph on the generator device (`frame_gen::RifeNet`) and on
// the CPU reference from the same inputs, and compares every blob.
//
//   rife_net_check <model-dir> [<width> <height>]
//
// The inputs are pseudo-random RGB, by default as large as the model's multiple
// (`RifeNet::multiple`), the time 0.5. Each blob's worst difference is measured against its
// largest reference value.
//
// Exit status: 0 when every blob agrees within `tolerance`, 1 otherwise or without a device.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "frame_gen/compute_pass.hpp"
#include "frame_gen/generator_gpu.hpp"
#include "frame_gen/ncnn_model.hpp"
#include "frame_gen/rife_net.hpp"
#include "lucent/log.h"
#include "rife_reference.hpp"

namespace {

using opensu::frame_gen::test::Blob;

/// fp32 summed in another order; the noise's steep edges turn the flows' rounding into warped
/// differences of a few thousandths, where a wrong index or padding is off by the whole value.
constexpr double tolerance = 1e-2;

Blob noise(int w, int h, std::uint32_t seed) {
    Blob b = opensu::frame_gen::test::blob(w, h, 3);
    std::uint32_t state = seed;
    for (float& value : b.data) {
        state = (state * 1664525U) + 1013904223U;
        value = static_cast<float>(state >> 8U) / static_cast<float>(1U << 24U);
    }
    return b;
}

int check(const std::filesystem::path& dir, std::optional<std::pair<int, int>> size) {
    const opensu::frame_gen::ncnn::Model model =
        opensu::frame_gen::ncnn::load(dir / "flownet.param", dir / "flownet.bin");
    opensu::frame_gen::GeneratorDevice gpu;
    opensu::frame_gen::RifeNet net{gpu, model};
    const auto multiple = static_cast<int>(net.multiple());
    const auto [w, h] = size.value_or(std::pair{multiple, multiple});
    lucent::info("rife", "{} on {} at {}x{}: inputs a multiple of {}", dir.filename().string(),
                 gpu.name(), w, h, multiple);
    const std::map<std::string, Blob> inputs{{"in0", noise(w, h, 1)},
                                             {"in1", noise(w, h, 2)},
                                             {"in2", opensu::frame_gen::test::blob(w, h, 1, 0.5F)}};
    const std::vector<Blob> expected = opensu::frame_gen::test::run(model, inputs);

    net.size(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
    for (const auto& [name, values] : inputs) {
        net.write(model.blob(name), values.data);
    }
    VkCommandBuffer cmd = gpu.Begin();
    if (cmd == VK_NULL_HANDLE) {
        lucent::error("rife", "no command buffer");
        return 1;
    }
    std::vector<opensu::frame_gen::NamedWork> work;
    net.record(cmd, work);
    gpu.Wait(gpu.Submit(cmd));

    int failed = 0;
    double worst = 0;
    std::string worstName;
    for (std::size_t b = 0; b < model.blobs.size(); ++b) {
        const Blob& want = expected[b];
        const std::vector<float> got = net.read(static_cast<int>(b));
        const opensu::frame_gen::RifeNet::Shape shape = net.shape(static_cast<int>(b));
        if (shape.width != want.w || shape.height != want.h || shape.channels != want.c) {
            lucent::error("rife", "{}: {}x{}x{} on the device, {}x{}x{} on the CPU", model.blobs[b],
                          shape.width, shape.height, shape.channels, want.w, want.h, want.c);
            ++failed;
            continue;
        }
        double largest = 1e-3;
        double difference = 0;
        for (std::size_t i = 0; i < got.size(); ++i) {
            largest = std::max(largest, static_cast<double>(std::fabs(want.data[i])));
            difference =
                std::max(difference, static_cast<double>(std::fabs(got[i] - want.data[i])));
        }
        const double relative = difference / largest;
        if (relative > worst) {
            worst = relative;
            worstName = model.blobs[b];
        }
        if (!(relative <= tolerance)) {
            lucent::error("rife", "{}: off by {} of {}", model.blobs[b], relative, largest);
            ++failed;
        }
    }
    lucent::info("rife", "{} blobs, {} off; the worst {} by {}", model.blobs.size(), failed,
                 worstName, worst);
    return failed == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    const std::span<char*> args{argv, static_cast<std::size_t>(argc)};
    if (args.size() != 2 && args.size() != 4) {
        lucent::error("rife", "usage: rife_net_check <model-dir> [<width> <height>]");
        return 1;
    }
    try {
        std::optional<std::pair<int, int>> size;
        if (args.size() == 4) {
            size = std::pair{std::stoi(args[2]), std::stoi(args[3])};
        }
        return check(args[1], size);
    } catch (const std::exception& failure) {
        lucent::error("rife", "{}", failure.what());
        return 1;
    }
}
