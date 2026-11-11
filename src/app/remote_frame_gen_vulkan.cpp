#include "remote_frame_gen.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "frame_gen/generator_gpu.hpp"
#include "frame_gen/interpolator.hpp"
#include "framegen/FramePacer.h"
#include "framegen/LosslessModules.h"
#include "lucent/log.h"
#include "stream/stream_wire.hpp"

namespace opensu::app {
namespace {

/// What `pack` writes first.
constexpr std::string_view packMagic = "OSULSFG1";
/// Real frames whose images and outputs are kept: the interval shown, the one after, and the next.
constexpr std::uint64_t bufferedFrames = 3;
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

framegen::PacingSettings_t pacingOf(const settings::FrameGenSettings& settings) {
    constexpr double thousand = 1000.0;
    if (settings.mode == settings::FrameGenMode::Fixed) {
        return framegen::PacingSettings_t{.eMode = framegen::PacingMode::Fixed,
                                          .flRatio = settings.ratioThousandths / thousand,
                                          .flTargetFps = 0.0,
                                          .uMaxGenerated = 3,
                                          .flToleranceFps = 0.0};
    }
    return framegen::PacingSettings_t{
        .eMode = framegen::PacingMode::Adaptive,
        .flRatio = 1.0,
        .flTargetFps = settings.targetThousandths / thousand,
        .uMaxGenerated = std::clamp(settings.maxGenerated, 1U, framegen::k_uMaxGeneratedLimit),
        .flToleranceFps = settings.toleranceThousandths / thousand};
}

class VulkanFrameGen final : public RemoteFrameGen {
  public:
    VulkanFrameGen(const settings::FrameGenSettings& settings,
                   std::shared_ptr<const framegen::LosslessModules_t> modules) {
        pacer_.Configure(pacingOf(settings));
        std::string why;
        generator_ = frame_gen::lossless(
            gpu_,
            framegen::LosslessSettings_t{
                .sDllPath = {},
                .pModules = std::move(modules),
                .eSizeMode = settings.performance ? framegen::LosslessSizeMode::Performance
                                                  : framegen::LosslessSizeMode::Balanced,
                .flFlowScale = std::clamp(static_cast<float>(settings.flowScalePercent) / 100.0F,
                                          framegen::k_flLosslessMinFlowScale, 1.0F),
                .bUiDetection = true},
            why);
        if (!generator_) {
            throw std::runtime_error{why};
        }
        lucent::info("framegen", "generating on {}, {} size mode, up to {} per real frame",
                     gpu_.name(), settings.performance ? "performance" : "balanced",
                     pacer_.GetMaxGeneratedPerFrame());
    }
    ~VulkanFrameGen() override {
        // A lost device has nothing left to wait for; the images go either way.
        static_cast<void>(gpu_.idle());
    }
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
            if (width != width_ || height != height_) {
                begin(width, height);
            }
            return add(std::move(frame), refreshNs);
        } catch (const std::runtime_error& failure) {
            lucent::error("framegen", "frame generation stops: {}", failure.what());
            return false;
        }
    }

    std::optional<stream::GuestVideo::Frame> present(std::uint64_t nowUs,
                                                     std::uint64_t refreshNs) override {
        if (width_ == 0) {
            return std::nullopt;
        }
        const std::uint64_t completed = gpu_.Completed();
        const std::optional<framegen::PacedOutput_t> output = pacer_.OnVBlank(
            nowUs * nanosPerMicro, refreshNs, [this, completed](const framegen::PacedOutput_t& o) {
                const std::optional<std::uint64_t> seq = sequenceOf(o);
                return !o.oGenerated || (seq && *seq <= completed);
            });
        if (!output) {
            return std::nullopt;
        }
        ++shown_;
        const Real& interval = reals_[output->ulInterval % bufferedFrames];
        if (!output->oGenerated) {
            const Real& real = reals_[output->ulRealFrame % bufferedFrames];
            return real.frame;
        }
        ++shownGenerated_;
        const std::span<const std::uint8_t> bytes =
            readbacks_[slotOf(output->ulInterval, *output->oGenerated)]->bytes();
        return stream::GuestVideo::Frame{.number = interval.frame.number,
                                         .picture =
                                             stream::Picture{.width = static_cast<int>(width_),
                                                             .height = static_cast<int>(height_),
                                                             .rgba = {bytes.begin(), bytes.end()},
                                                             .dmabuf = std::nullopt,
                                                             .frame = std::nullopt},
                                         .capturedUs = interval.frame.capturedUs,
                                         .decodedUs = interval.frame.decodedUs};
    }

    std::string summary() const override {
        return std::format("framegen real={} shown={} generated={} dropped={} state={}", realIn_,
                           shown_, shownGenerated_, pacer_.GetDroppedOutputs(),
                           state_ ? framegen::PacingStateName(*state_) : "none");
    }

  private:
    /// A real frame kept for the intervals that show it, the submissions of its interval's
    /// generated images, and the last submission that uses its slot's image or buffers.
    struct Real {
        stream::GuestVideo::Frame frame;
        std::vector<std::uint64_t> generated;
        std::uint64_t busy{0};
    };

    [[nodiscard]] std::size_t slotOf(std::uint64_t interval, std::uint32_t generated) const {
        return static_cast<std::size_t>((interval % bufferedFrames) *
                                        pacer_.GetMaxGeneratedPerFrame()) +
               generated;
    }

    [[nodiscard]] std::optional<std::uint64_t>
    sequenceOf(const framegen::PacedOutput_t& output) const {
        const Real& interval = reals_[output.ulInterval % bufferedFrames];
        if (!output.oGenerated || *output.oGenerated >= interval.generated.size()) {
            return std::nullopt;
        }
        return interval.generated[*output.oGenerated];
    }

    /// A new session of `width` by `height`: every image and real frame before it forgotten.
    void begin(std::uint32_t width, std::uint32_t height) {
        frame_gen::check(gpu_.idle(), "vkDeviceWaitIdle");
        sources_.clear();
        uploads_.clear();
        outputs_.clear();
        readbacks_.clear();
        reals_ = {};
        width_ = 0;
        height_ = 0;
        const VkDeviceSize bytes = VkDeviceSize{width} * height * bytesPerPixel;
        for (std::uint64_t i = 0; i < bufferedFrames; ++i) {
            sources_.push_back(std::make_unique<frame_gen::GeneratorImage>(gpu_, width, height));
            uploads_.push_back(std::make_unique<frame_gen::HostBuffer>(
                gpu_, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
        }
        const std::size_t outputs = bufferedFrames * pacer_.GetMaxGeneratedPerFrame();
        for (std::size_t i = 0; i < outputs; ++i) {
            outputs_.push_back(std::make_unique<frame_gen::GeneratorImage>(gpu_, width, height));
            readbacks_.push_back(std::make_unique<frame_gen::HostBuffer>(
                gpu_, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
        }
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            throw std::runtime_error{"no command buffer for the images"};
        }
        for (const auto& image : sources_) {
            image->toGeneral(cmd);
        }
        for (const auto& image : outputs_) {
            image->toGeneral(cmd);
        }
        gpu_.Wait(gpu_.Submit(cmd));
        pacer_.Reset();
        frames_ = 0;
        lastAdded_.reset();
        width_ = width;
        height_ = height;
        lucent::info("framegen", "session at {}x{}", width, height);
    }

    bool add(stream::GuestVideo::Frame frame, std::uint64_t refreshNs) {
        const std::uint64_t index = frames_;
        if (index >= 1) {
            // The images the upload and this interval overwrite.
            const std::uint64_t oldest =
                index > bufferedFrames - 1 ? index - (bufferedFrames - 1) : 0;
            pacer_.DropOlderThan(oldest, oldest);
        }
        Real& slot = reals_[index % bufferedFrames];
        const frame_gen::GeneratorImage& source = *sources_[index % bufferedFrames];
        const frame_gen::HostBuffer& upload = *uploads_[index % bufferedFrames];
        // Whatever still uses the slot's image or buffers finishes first.
        gpu_.Wait(slot.busy);
        const std::span<std::uint8_t> staged = upload.bytes();
        std::memcpy(staged.data(), frame.picture.rgba.data(), staged.size());
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            lucent::error("framegen", "no command buffer for frame {}", frame.number);
            return false;
        }
        frame_gen::memoryBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkBufferImageCopy region = frame_gen::wholeImage(width_, height_);
        vkCmdCopyBufferToImage(cmd, upload.buffer(), source.image(), VK_IMAGE_LAYOUT_GENERAL, 1,
                               &region);
        slot.busy = gpu_.Submit(cmd);
        slot.generated.clear();

        const std::uint64_t readyNs = frame.capturedUs * nanosPerMicro;
        // When it reached frame generation, which places it on the output timeline.
        const std::uint64_t arrivalNs = stream::wire::nowMicros() * nanosPerMicro;
        slot.frame = std::move(frame);
        const framegen::IntervalPlan_t plan =
            pacer_.OnRealFrame(index, readyNs, arrivalNs, refreshNs);
        if (plan.eState == framegen::PacingState::Generating) {
            const bool follows = lastAdded_ && *lastAdded_ + 1 == index;
            if (!follows && !generator_->begin(width_, height_)) {
                lucent::error("framegen", "the generator does not restart at frame {}",
                              slot.frame.number);
                return false;
            }
            const std::optional<framegen::Submission_t> added = generator_->add(source.view());
            if (!added || !added->bComplete) {
                lucent::error("framegen", "frame {} did not go into the generator",
                              slot.frame.number);
                return false;
            }
            slot.busy = added->ulSeqNo;
            lastAdded_ = index;
        }
        if (!state_ || *state_ != plan.eState) {
            lucent::info("framegen", "pacing {} at frame {}: base {:.2f} fps, target {:.2f} fps",
                         framegen::PacingStateName(plan.eState), slot.frame.number, plan.flBaseFps,
                         plan.flTargetFps);
            state_ = plan.eState;
        }
        for (std::size_t i = 0; i < plan.flTimestamps.size(); ++i) {
            const std::size_t at = slotOf(index, static_cast<std::uint32_t>(i));
            const frame_gen::GeneratorImage& output = *outputs_[at];
            const frame_gen::HostBuffer& readback = *readbacks_[at];
            const framegen::OutputHooks_t hooks{
                .before = {}, .after = [this, &output, &readback](VkCommandBuffer recording) {
                    frame_gen::memoryBarrier(
                        recording, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                    const VkBufferImageCopy copy = frame_gen::wholeImage(width_, height_);
                    vkCmdCopyImageToBuffer(recording, output.image(), VK_IMAGE_LAYOUT_GENERAL,
                                           readback.buffer(), 1, &copy);
                    frame_gen::memoryBarrier(recording, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                             VK_ACCESS_TRANSFER_WRITE_BIT,
                                             VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
                }};
            const std::optional<framegen::Submission_t> made =
                generator_->generate(plan.flTimestamps[i], output.view(), hooks);
            if (!made || !made->bComplete) {
                lucent::error("framegen", "no image generated before frame {}", slot.frame.number);
                return false;
            }
            slot.generated.push_back(made->ulSeqNo);
            slot.busy = made->ulSeqNo;
        }
        ++frames_;
        ++realIn_;
        return true;
    }

    frame_gen::GeneratorDevice gpu_;
    std::unique_ptr<frame_gen::Interpolator> generator_;
    framegen::CFramePacer pacer_;
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::vector<std::unique_ptr<frame_gen::GeneratorImage>> sources_;
    std::vector<std::unique_ptr<frame_gen::HostBuffer>> uploads_;
    std::vector<std::unique_ptr<frame_gen::GeneratorImage>> outputs_;
    std::vector<std::unique_ptr<frame_gen::HostBuffer>> readbacks_;
    std::array<Real, bufferedFrames> reals_{};
    /// Real frames since the session began, the last of them the generator took, and its state.
    std::uint64_t frames_{0};
    std::optional<std::uint64_t> lastAdded_;
    std::optional<framegen::PacingState> state_;
    std::uint64_t realIn_{0};
    std::uint64_t shown_{0};
    std::uint64_t shownGenerated_{0};
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
