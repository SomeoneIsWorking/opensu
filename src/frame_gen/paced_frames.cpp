#include "frame_gen/paced_frames.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "lucent/log.h"

namespace opensu::frame_gen {
namespace {

constexpr std::uint32_t bytesPerPixel = 4;

} // namespace

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

PacedFrames::PacedFrames(GeneratorDevice& gpu, std::unique_ptr<Interpolator> generator,
                         const framegen::PacingSettings_t& pacing, SourcePixel pixel)
    : gpu_{gpu}, generator_{std::move(generator)}, pixel_{pixel} {
    pacer_.Configure(pacing);
}

PacedFrames::~PacedFrames() {
    // A lost device has nothing left to wait for; the images go either way.
    static_cast<void>(gpu_.idle());
}

std::size_t PacedFrames::slotOf(std::uint64_t interval, std::uint32_t generated) const {
    return static_cast<std::size_t>((interval % bufferedFrames) *
                                    pacer_.GetMaxGeneratedPerFrame()) +
           generated;
}

std::optional<std::uint64_t> PacedFrames::sequenceOf(const framegen::PacedOutput_t& output) const {
    const Slot& interval = slots_[output.ulInterval % bufferedFrames];
    if (!output.oGenerated || *output.oGenerated >= interval.generated.size()) {
        return std::nullopt;
    }
    return interval.generated[*output.oGenerated];
}

void PacedFrames::begin(std::uint32_t width, std::uint32_t height) {
    check(gpu_.idle(), "vkDeviceWaitIdle");
    sources_.clear();
    uploads_.clear();
    outputs_.clear();
    readbacks_.clear();
    slots_ = {};
    width_ = 0;
    height_ = 0;
    const VkDeviceSize bytes = VkDeviceSize{width} * height * bytesPerPixel;
    for (std::uint64_t i = 0; i < bufferedFrames; ++i) {
        sources_.push_back(std::make_unique<GeneratorImage>(gpu_, width, height));
        uploads_.push_back(
            std::make_unique<HostBuffer>(gpu_, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    }
    const std::size_t outputs = bufferedFrames * pacer_.GetMaxGeneratedPerFrame();
    for (std::size_t i = 0; i < outputs; ++i) {
        outputs_.push_back(std::make_unique<GeneratorImage>(gpu_, width, height));
        readbacks_.push_back(
            std::make_unique<HostBuffer>(gpu_, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
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

std::uint64_t PacedFrames::add(std::span<const std::uint8_t> rgba, std::uint32_t width,
                               std::uint32_t height, std::uint64_t readyNs, std::uint64_t arrivalNs,
                               std::uint64_t refreshNs) {
    if (rgba.size() != std::size_t{width} * height * bytesPerPixel) {
        throw std::runtime_error{"a real frame is not in RGBA of its size"};
    }
    if (width != width_ || height != height_) {
        begin(width, height);
    }
    const std::uint64_t index = frames_;
    if (index >= 1) {
        // The images the upload and this interval overwrite.
        const std::uint64_t oldest = index > bufferedFrames - 1 ? index - (bufferedFrames - 1) : 0;
        pacer_.DropOlderThan(oldest, oldest);
    }
    Slot& slot = slots_[index % bufferedFrames];
    const GeneratorImage& source = *sources_[index % bufferedFrames];
    const HostBuffer& upload = *uploads_[index % bufferedFrames];
    // Whatever still uses the slot's image or buffers finishes first.
    gpu_.Wait(slot.busy);
    const std::span<std::uint8_t> staged = upload.bytes();
    std::memcpy(staged.data(), rgba.data(), staged.size());
    VkCommandBuffer cmd = gpu_.Begin();
    if (cmd == VK_NULL_HANDLE) {
        throw std::runtime_error{"no command buffer for a real frame"};
    }
    memoryBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    const VkBufferImageCopy region = wholeImage(width_, height_);
    vkCmdCopyBufferToImage(cmd, upload.buffer(), source.image(), VK_IMAGE_LAYOUT_GENERAL, 1,
                           &region);
    slot.busy = gpu_.Submit(cmd);
    slot.generated.clear();

    const framegen::IntervalPlan_t plan = pacer_.OnRealFrame(index, readyNs, arrivalNs, refreshNs);
    if (plan.eState == framegen::PacingState::Generating) {
        const bool follows = lastAdded_ && *lastAdded_ + 1 == index;
        if (!follows && !generator_->begin(width_, height_, pixel_)) {
            throw std::runtime_error{"the generator does not restart"};
        }
        const std::optional<framegen::Submission_t> added = generator_->add(source.view());
        if (!added || !added->bComplete) {
            throw std::runtime_error{"a real frame did not go into the generator"};
        }
        slot.busy = added->ulSeqNo;
        lastAdded_ = index;
    }
    if (!state_ || *state_ != plan.eState) {
        lucent::info("framegen", "pacing {} at real frame {}: base {:.2f} fps, target {:.2f} fps",
                     framegen::PacingStateName(plan.eState), realIn_, plan.flBaseFps,
                     plan.flTargetFps);
        state_ = plan.eState;
    }
    for (std::size_t i = 0; i < plan.flTimestamps.size(); ++i) {
        const std::size_t at = slotOf(index, static_cast<std::uint32_t>(i));
        const GeneratorImage& output = *outputs_[at];
        const HostBuffer& readback = *readbacks_[at];
        const framegen::OutputHooks_t hooks{
            .before = {}, .after = [this, &output, &readback](VkCommandBuffer recording) {
                memoryBarrier(recording, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_ACCESS_TRANSFER_READ_BIT);
                const VkBufferImageCopy copy = wholeImage(width_, height_);
                vkCmdCopyImageToBuffer(recording, output.image(), VK_IMAGE_LAYOUT_GENERAL,
                                       readback.buffer(), 1, &copy);
                memoryBarrier(recording, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                              VK_ACCESS_HOST_READ_BIT);
            }};
        const std::optional<framegen::Submission_t> made =
            generator_->generate(plan.flTimestamps[i], output.view(), hooks);
        if (!made || !made->bComplete) {
            throw std::runtime_error{"no image generated before a real frame"};
        }
        slot.generated.push_back(made->ulSeqNo);
        slot.busy = made->ulSeqNo;
    }
    ++frames_;
    ++realIn_;
    return index;
}

std::optional<Shown> PacedFrames::vblank(std::uint64_t nowNs, std::uint64_t refreshNs) {
    if (width_ == 0) {
        return std::nullopt;
    }
    const std::uint64_t completed = gpu_.Completed();
    const std::optional<framegen::PacedOutput_t> output =
        pacer_.OnVBlank(nowNs, refreshNs, [this, completed](const framegen::PacedOutput_t& o) {
            const std::optional<std::uint64_t> seq = sequenceOf(o);
            return !o.oGenerated || (seq && *seq <= completed);
        });
    if (!output) {
        return std::nullopt;
    }
    ++shown_;
    if (!output->oGenerated) {
        return Shown{.real = output->ulRealFrame, .generated = std::nullopt};
    }
    ++shownGenerated_;
    return Shown{.real = output->ulInterval,
                 .generated = std::span<const std::uint8_t>{
                     readbacks_[slotOf(output->ulInterval, *output->oGenerated)]->bytes()}};
}

} // namespace opensu::frame_gen
