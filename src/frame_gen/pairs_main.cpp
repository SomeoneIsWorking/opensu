// Command opensu-framegen-pairs regenerates the frames a lower rate leaves out, for
// tools/framegen_quality to score against the real ones: of the PNG frames in --in, sorted by
// name, every --step-th is a real frame given to the generator, and the frames between two real
// ones are generated at their times and written into --out under their own names.
//
//   opensu-framegen-pairs --generator lossless [--dll <Lossless.dll>] [--performance]
//                         [--flow-scale <percent>] [--step N] --in <dir> --out <dir>
//   opensu-framegen-pairs --generator flow [--source-pixel <w>x<h> | --core-frames]
//                         [--step N] --in <dir> --out <dir>
//
// --source-pixel is how many frame pixels one of the content's takes (a core's frame scaled to
// the window), 1x1 by default. --core-frames says the frames are a core's own, which flow works on
// scaled up by `frame_gen::flowContentScale`, as the runner has it.
//
// Without --dll, the player's Lossless.dll is the one the shell finds in their Steam libraries.
//
// At the end, the GPU time of each real frame's ingest and each generated frame (`[perf]` lines).
//
// Exit status: 0 when every frame between two real ones was written, 1 otherwise.
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "config/config.hpp"
#include "fileio/atomic_write.hpp"
#include "frame_gen/generator_gpu.hpp"
#include "frame_gen/interpolator.hpp"
#include "gpu2d/image/decode.hpp"
#include "gpu2d/image/png_encoder.hpp"
#include "library/lossless_scaling.hpp"
#include "library/steam.hpp"
#include "lucent/log.h"

namespace {

using opensu::frame_gen::GeneratorDevice;
using opensu::frame_gen::GeneratorImage;
using opensu::frame_gen::HostBuffer;

constexpr std::uint32_t bytesPerPixel = 4;
constexpr float percent = 100.0F;

struct Options {
    std::string generator;
    std::filesystem::path dll;
    bool performance{false};
    unsigned flowScale{100};
    unsigned step{2};
    bool coreFrames{false};
    opensu::frame_gen::SourcePixel pixel;
    std::filesystem::path in;
    std::filesystem::path out;
};

std::optional<unsigned> number(std::string_view text) {
    unsigned value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

/// `<w>x<h>`, each at least 1.
std::optional<opensu::frame_gen::SourcePixel> pixelOf(std::string_view text) {
    const std::size_t x = text.find('x');
    if (x == std::string_view::npos) {
        return std::nullopt;
    }
    opensu::frame_gen::SourcePixel pixel;
    const std::string_view width = text.substr(0, x);
    const std::string_view height = text.substr(x + 1);
    const auto [widthEnd, widthError] =
        std::from_chars(width.data(), width.data() + width.size(), pixel.width);
    const auto [heightEnd, heightError] =
        std::from_chars(height.data(), height.data() + height.size(), pixel.height);
    if (widthError != std::errc{} || widthEnd != width.data() + width.size() ||
        heightError != std::errc{} || heightEnd != height.data() + height.size() ||
        pixel.width < 1.0F || pixel.height < 1.0F) {
        return std::nullopt;
    }
    return pixel;
}

std::optional<Options> parse(std::span<const std::string> args) {
    Options options;
    for (std::size_t at = 0; at < args.size(); ++at) {
        const std::string_view arg = args[at];
        if (arg == "--performance" || arg == "--core-frames") {
            (arg == "--performance" ? options.performance : options.coreFrames) = true;
            continue;
        }
        if (at + 1 >= args.size()) {
            lucent::error("framegen", "{} needs a value", arg);
            return std::nullopt;
        }
        const std::string& value = args[++at];
        if (arg == "--generator") {
            options.generator = value;
        } else if (arg == "--dll") {
            options.dll = value;
        } else if (arg == "--in") {
            options.in = value;
        } else if (arg == "--out") {
            options.out = value;
        } else if (arg == "--source-pixel") {
            const std::optional<opensu::frame_gen::SourcePixel> pixel = pixelOf(value);
            if (!pixel) {
                lucent::error("framegen", "--source-pixel takes <w>x<h>, each 1 or more, not {}",
                              value);
                return std::nullopt;
            }
            options.pixel = *pixel;
        } else if (arg == "--flow-scale" || arg == "--step") {
            const std::optional<unsigned> given = number(value);
            if (!given || *given == 0) {
                lucent::error("framegen", "{} takes a positive whole number, not {}", arg, value);
                return std::nullopt;
            }
            (arg == "--step" ? options.step : options.flowScale) = *given;
        } else {
            lucent::error("framegen", "unexpected argument {}", arg);
            return std::nullopt;
        }
    }
    if ((options.generator != "lossless" && options.generator != "flow") || options.in.empty() ||
        options.out.empty() || options.step < 2) {
        lucent::error("framegen", "usage: opensu-framegen-pairs --generator lossless|flow "
                                  "[--dll <dll>] [--performance] [--flow-scale <percent>] "
                                  "[--source-pixel <w>x<h> | --core-frames] [--step N] "
                                  "--in <dir> --out <dir> "
                                  "(--step 2 or more)");
        return std::nullopt;
    }
    return options;
}

std::unique_ptr<opensu::frame_gen::Interpolator> generatorOf(Options options,
                                                             GeneratorDevice& gpu) {
    std::string why;
    if (options.generator == "flow") {
        const std::uint32_t scale = options.coreFrames ? opensu::frame_gen::flowContentScale : 1;
        std::unique_ptr<opensu::frame_gen::Interpolator> made =
            opensu::frame_gen::flow(gpu, scale, why);
        if (!made) {
            lucent::error("framegen", "no flow generator: {}", why);
            return made;
        }
        lucent::info("framegen", "flow works on the frames scaled up by {}", scale);
        return made;
    }
    if (options.dll.empty()) {
        const opensu::config::Config config = opensu::config::read();
        const std::optional<std::filesystem::path> found = opensu::library::findLosslessDll(
            opensu::library::steam::Library::discover(config.home, config.steamRoots));
        if (!found) {
            lucent::error("framegen",
                          "no Lossless.dll in the Steam libraries; name one with --dll");
            return nullptr;
        }
        options.dll = *found;
    }
    lucent::info("framegen", "Lossless shaders from {}", options.dll.string());
    std::unique_ptr<opensu::frame_gen::Interpolator> made = opensu::frame_gen::lossless(
        gpu,
        framegen::LosslessSettings_t{
            .sDllPath = options.dll.string(),
            .pModules = nullptr,
            .eSizeMode = options.performance ? framegen::LosslessSizeMode::Performance
                                             : framegen::LosslessSizeMode::Balanced,
            .flFlowScale = std::clamp(static_cast<float>(options.flowScale) / percent,
                                      framegen::k_flLosslessMinFlowScale, 1.0F),
            .bUiDetection = true},
        why);
    if (!made) {
        lucent::error("framegen", "no {} generator: {}", options.generator, why);
    }
    return made;
}

/// Real frames uploaded in turn into three images, as remote play's guest keeps them, and the
/// frames between read back one at a time.
class Pairs {
  public:
    Pairs(GeneratorDevice& gpu, opensu::frame_gen::Interpolator& generator, std::uint32_t width,
          std::uint32_t height, opensu::frame_gen::SourcePixel pixel)
        : gpu_{gpu}, generator_{generator}, width_{width}, height_{height},
          output_{gpu, width, height}, readback_{gpu, VkDeviceSize{width} * height * bytesPerPixel,
                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT} {
        for (auto& source : sources_) {
            source = std::make_unique<GeneratorImage>(gpu, width, height);
            uploads_.push_back(
                std::make_unique<HostBuffer>(gpu, VkDeviceSize{width} * height * bytesPerPixel,
                                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
        }
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            throw std::runtime_error{"no command buffer for the images"};
        }
        for (const auto& source : sources_) {
            source->toGeneral(cmd);
        }
        output_.toGeneral(cmd);
        gpu_.Wait(gpu_.Submit(cmd));
        if (!generator_.begin(width, height, pixel)) {
            throw std::runtime_error{"the generator does not begin a session"};
        }
    }

    /// The next real frame's RGBA.
    void add(std::span<const std::uint8_t> rgba) {
        const std::size_t slot = added_ % sources_.size();
        const std::span<std::uint8_t> staged = uploads_[slot]->bytes();
        std::memcpy(staged.data(), rgba.data(), staged.size());
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            throw std::runtime_error{"no command buffer for a real frame"};
        }
        opensu::frame_gen::memoryBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                         VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkBufferImageCopy region = opensu::frame_gen::wholeImage(width_, height_);
        vkCmdCopyBufferToImage(cmd, uploads_[slot]->buffer(), sources_[slot]->image(),
                               VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        gpu_.Wait(gpu_.Submit(cmd));
        const std::optional<framegen::Submission_t> added = generator_.add(sources_[slot]->view());
        if (!added || !added->bComplete) {
            throw std::runtime_error{std::format("real frame {} did not go in", added_)};
        }
        gpu_.Wait(added->ulSeqNo);
        ++added_;
    }

    /// The scene at `t` between the last two real frames, in RGBA.
    std::span<const std::uint8_t> between(float t) {
        const framegen::OutputHooks_t hooks{
            .before = {}, .after = [this](VkCommandBuffer recording) {
                opensu::frame_gen::memoryBarrier(
                    recording, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                const VkBufferImageCopy copy = opensu::frame_gen::wholeImage(width_, height_);
                vkCmdCopyImageToBuffer(recording, output_.image(), VK_IMAGE_LAYOUT_GENERAL,
                                       readback_.buffer(), 1, &copy);
                opensu::frame_gen::memoryBarrier(
                    recording, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
            }};
        const std::optional<framegen::Submission_t> made =
            generator_.generate(t, output_.view(), hooks);
        if (!made || !made->bComplete) {
            throw std::runtime_error{
                std::format("nothing generated at {} after real frame {}", t, added_)};
        }
        gpu_.Wait(made->ulSeqNo);
        return readback_.bytes();
    }

  private:
    GeneratorDevice& gpu_;
    opensu::frame_gen::Interpolator& generator_;
    std::uint32_t width_;
    std::uint32_t height_;
    std::array<std::unique_ptr<GeneratorImage>, 3> sources_;
    std::vector<std::unique_ptr<HostBuffer>> uploads_;
    GeneratorImage output_;
    HostBuffer readback_;
    std::uint64_t added_{0};
};

std::optional<gpu2d::image::Bitmap> load(const std::filesystem::path& file) {
    std::string error;
    std::optional<gpu2d::image::Bitmap> bitmap = gpu2d::image::decodeFile(file, error);
    if (!bitmap) {
        lucent::error("framegen", "{}: {}", file.string(), error);
    }
    return bitmap;
}

int run(const Options& options) {
    std::vector<std::filesystem::path> frames;
    for (const auto& entry : std::filesystem::directory_iterator{options.in}) {
        if (entry.path().extension() == ".png") {
            frames.push_back(entry.path());
        }
    }
    std::ranges::sort(frames);
    if (frames.size() <= options.step) {
        lucent::error("framegen", "{} has {} frames, fewer than two real ones", options.in.string(),
                      frames.size());
        return 1;
    }
    GeneratorDevice gpu;
    gpu.measure();
    std::unique_ptr<opensu::frame_gen::Interpolator> generator = generatorOf(options, gpu);
    if (!generator) {
        return 1;
    }
    const std::optional<gpu2d::image::Bitmap> first = load(frames.front());
    if (!first) {
        return 1;
    }
    const auto width = static_cast<std::uint32_t>(first->width);
    const auto height = static_cast<std::uint32_t>(first->height);
    Pairs pairs{gpu, *generator, width, height, options.pixel};
    pairs.add(first->rgba);
    std::filesystem::create_directories(options.out);
    std::size_t written = 0;
    for (std::size_t real = options.step; real < frames.size(); real += options.step) {
        const std::optional<gpu2d::image::Bitmap> next = load(frames[real]);
        if (!next) {
            return 1;
        }
        if (next->width != first->width || next->height != first->height) {
            lucent::error("framegen", "{} is {}x{}, not {}x{}", frames[real].string(), next->width,
                          next->height, width, height);
            return 1;
        }
        pairs.add(next->rgba);
        for (unsigned left = 1; left < options.step; ++left) {
            const float t = static_cast<float>(left) / static_cast<float>(options.step);
            std::string error;
            const std::string png = gpu2d::image::encodePng(pairs.between(t), width, height, error);
            const std::filesystem::path file =
                options.out / frames[real - options.step + left].filename();
            if (png.empty() || !opensu::fileio::writeWhole(file, png, error)) {
                lucent::error("framegen", "cannot write {}: {}", file.string(), error);
                return 1;
            }
            ++written;
        }
    }
    lucent::info("framegen", "{} generated {} frames on {} into {}", options.generator, written,
                 gpu.name(), options.out.string());
    gpu.spans()->report();
    return 0;
}

} // namespace

int main(int argc, char** argv) try {
    const std::vector<std::string> args(argv + 1, argv + argc);
    const std::optional<Options> options = parse(args);
    if (!options) {
        return 1;
    }
    return run(*options);
} catch (const std::exception& failure) {
    lucent::error("framegen", "{}", failure.what());
    return 1;
}
