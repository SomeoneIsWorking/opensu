#include "remote_frame_gen.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

#include "frame_gen/generator_gpu.hpp"
#include "frame_gen/interpolator.hpp"
#include "frame_gen/paced_frames.hpp"
#include "framegen/FramePacer.h"
#include "framegen/LosslessModules.h"
#include "lucent/log.h"
#include "stream/stream_wire.hpp"

namespace opensu::app {
namespace {

/// What `pack` writes first.
constexpr std::string_view packMagic = "OSULSFG1";
constexpr std::uint64_t nanosPerMicro = 1000;
constexpr std::uint32_t bytesPerPixel = 4;

void put(std::string& out, std::uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        out.push_back(static_cast<char>((value >> (8 * i)) & 0xFFU));
    }
}

/// Reads `packed` from the front; fails once anything is missing.
class Unpacker {
  public:
    explicit Unpacker(std::string_view packed) : rest_{packed} {
    }
    std::uint64_t take(int bytes) {
        if (rest_.size() < static_cast<std::size_t>(bytes)) {
            ok_ = false;
            return 0;
        }
        std::uint64_t value = 0;
        for (int i = 0; i < bytes; ++i) {
            value |= std::uint64_t{static_cast<unsigned char>(rest_[static_cast<std::size_t>(i)])}
                     << (8 * i);
        }
        rest_.remove_prefix(static_cast<std::size_t>(bytes));
        return value;
    }
    std::string_view bytes(std::size_t count) {
        if (rest_.size() < count) {
            ok_ = false;
            return {};
        }
        const std::string_view taken = rest_.substr(0, count);
        rest_.remove_prefix(count);
        return taken;
    }
    [[nodiscard]] bool whole() const noexcept {
        return ok_ && rest_.empty();
    }

  private:
    std::string_view rest_;
    bool ok_{true};
};

std::optional<framegen::LosslessModules_t> unpack(std::string_view packed) {
    Unpacker in{packed};
    if (in.bytes(packMagic.size()) != packMagic) {
        return std::nullopt;
    }
    framegen::LosslessModules_t modules;
    modules.sFileVersion = std::string{in.bytes(in.take(4))};
    modules.ulDllSize = in.take(8);
    const std::uint64_t count = in.take(4);
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto id = static_cast<std::uint32_t>(in.take(4));
        const std::string_view data = in.bytes(in.take(4));
        modules.resources[id] = std::vector<std::uint8_t>(data.begin(), data.end());
    }
    if (!in.whole()) {
        return std::nullopt;
    }
    return modules;
}

/// The Lossless generator on `gpu`, its sizes from the game's settings; throws when it cannot run.
std::unique_ptr<frame_gen::Interpolator>
losslessOf(frame_gen::GeneratorDevice& gpu, const settings::FrameGenSettings& settings,
           std::shared_ptr<const framegen::LosslessModules_t> modules) {
    std::string why;
    std::unique_ptr<frame_gen::Interpolator> generator = frame_gen::lossless(
        gpu,
        framegen::LosslessSettings_t{
            .sDllPath = {},
            .pModules = std::move(modules),
            .eSizeMode = settings.performance ? framegen::LosslessSizeMode::Performance
                                              : framegen::LosslessSizeMode::Balanced,
            .flFlowScale = std::clamp(static_cast<float>(settings.flowScalePercent) / 100.0F,
                                      framegen::k_flLosslessMinFlowScale, 1.0F),
            .bUiDetection = true},
        why);
    if (!generator) {
        throw std::runtime_error{why};
    }
    return generator;
}

class VulkanFrameGen final : public RemoteFrameGen {
  public:
    // The stream does not say how its picture was scaled yet.
    VulkanFrameGen(const settings::FrameGenSettings& settings,
                   std::shared_ptr<const framegen::LosslessModules_t> modules)
        : frames_{gpu_, losslessOf(gpu_, settings, std::move(modules)),
                  frame_gen::pacingOf(settings), frame_gen::SourcePixel{}} {
        lucent::info("framegen", "generating on {}, {} size mode, up to {} per real frame",
                     gpu_.name(), settings.performance ? "performance" : "balanced",
                     frames_.maxGenerated());
    }
    ~VulkanFrameGen() override = default;
    VulkanFrameGen(const VulkanFrameGen&) = delete;
    VulkanFrameGen& operator=(const VulkanFrameGen&) = delete;
    VulkanFrameGen(VulkanFrameGen&&) = delete;
    VulkanFrameGen& operator=(VulkanFrameGen&&) = delete;

    bool real(stream::GuestVideo::Frame frame, std::uint64_t refreshNs) override {
        const stream::Picture& picture = frame.picture;
        const auto width = static_cast<std::uint32_t>(picture.width);
        const auto height = static_cast<std::uint32_t>(picture.height);
        if (picture.rgba.size() != std::size_t{width} * height * bytesPerPixel) {
            lucent::warn("framegen", "frame {} is not in CPU RGBA", frame.number);
            return false;
        }
        try {
            const std::uint64_t readyNs = frame.capturedUs * nanosPerMicro;
            // When it reached frame generation, which places it on the output timeline.
            const std::uint64_t arrivalNs = stream::wire::nowMicros() * nanosPerMicro;
            const std::uint64_t index =
                frames_.add(picture.rgba, width, height, readyNs, arrivalNs, refreshNs);
            reals_[index % frame_gen::bufferedFrames] = std::move(frame);
            return true;
        } catch (const std::runtime_error& failure) {
            lucent::error("framegen", "frame generation stops at frame {}: {}", frame.number,
                          failure.what());
            return false;
        }
    }

    std::optional<stream::GuestVideo::Frame> present(std::uint64_t nowUs,
                                                     std::uint64_t refreshNs) override {
        const std::optional<frame_gen::Shown> shown =
            frames_.vblank(nowUs * nanosPerMicro, refreshNs);
        if (!shown) {
            return std::nullopt;
        }
        const stream::GuestVideo::Frame& real = reals_[shown->real % frame_gen::bufferedFrames];
        if (!shown->generated) {
            return real;
        }
        return stream::GuestVideo::Frame{
            .number = real.number,
            .picture = stream::Picture{.width = static_cast<int>(frames_.width()),
                                       .height = static_cast<int>(frames_.height()),
                                       .rgba = {shown->generated->begin(), shown->generated->end()},
                                       .dmabuf = std::nullopt,
                                       .frame = std::nullopt},
            .capturedUs = real.capturedUs,
            .decodedUs = real.decodedUs};
    }

    std::string summary() const override {
        const std::optional<framegen::PacingState> state = frames_.state();
        return std::format("framegen real={} shown={} generated={} dropped={} state={}",
                           frames_.realIn(), frames_.shown(), frames_.shownGenerated(),
                           frames_.dropped(), state ? framegen::PacingStateName(*state) : "none");
    }

  private:
    frame_gen::GeneratorDevice gpu_;
    frame_gen::PacedFrames frames_;
    /// The real frames the kept intervals show, by their index.
    std::array<stream::GuestVideo::Frame, frame_gen::bufferedFrames> reals_{};
};

} // namespace

bool RemoteFrameGen::built() noexcept {
    return true;
}

std::optional<std::string> RemoteFrameGen::pack(const std::filesystem::path& dll,
                                                std::string& why) {
    const std::optional<framegen::LosslessModules_t> modules =
        framegen::ReadLosslessModules(dll.string(), why);
    if (!modules) {
        return std::nullopt;
    }
    std::string out{packMagic};
    put(out, modules->sFileVersion.size(), 4);
    out += modules->sFileVersion;
    put(out, modules->ulDllSize, 8);
    put(out, modules->resources.size(), 4);
    for (const auto& [id, data] : modules->resources) {
        put(out, id, 4);
        put(out, data.size(), 4);
        out.append(data.begin(), data.end());
    }
    return out;
}

std::unique_ptr<RemoteFrameGen> RemoteFrameGen::open(const settings::FrameGenSettings& settings,
                                                     std::string_view packed, std::string& why) {
    std::optional<framegen::LosslessModules_t> modules = unpack(packed);
    if (!modules) {
        why = "the host's shaders did not arrive whole";
        return nullptr;
    }
    try {
        return std::make_unique<VulkanFrameGen>(
            settings, std::make_shared<const framegen::LosslessModules_t>(std::move(*modules)));
    } catch (const std::runtime_error& failure) {
        why = failure.what();
        return nullptr;
    }
}

} // namespace opensu::app
