#include <array>
#include <cstdint>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "frame_gen/compute_pass.hpp"
#include "frame_gen/interpolator.hpp"

namespace opensu::frame_gen {
namespace {

constexpr std::uint32_t upCode[] = {
#include "scale_up.comp.inc"
};
constexpr std::uint32_t downCode[] = {
#include "scale_down.comp.inc"
};

constexpr std::uint32_t groupSide = 8;
constexpr std::uint64_t bytesPerPixel = 4;

std::uint32_t groups(std::uint32_t pixels) {
    return (pixels + groupSide - 1) / groupSide;
}

/// Another generator run on each frame scaled up nearest by `scale`, its frames between brought
/// back at the centres of the scaled pixels (`shaders/scale_up.comp`, `shaders/scale_down.comp`).
/// Each image the caller names gets a scaled image of its own, so the caller's buffering holds
/// for the scaled frames too.
class ScaledInterpolator final : public Interpolator {
  public:
    ScaledInterpolator(GeneratorDevice& gpu, std::unique_ptr<Interpolator> inner,
                       std::uint32_t scale)
        : gpu_{gpu}, inner_{std::move(inner)}, scale_{scale},
          image_{setLayout(gpu.device(), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1)},
          views_{gpu.device(), image_.get()} {
        const std::array<VkDescriptorSetLayout, 2> sets{image_.get(), image_.get()};
        up_ = computePass(gpu.device(), upCode, sets, sizeof(std::int32_t), nullptr,
                          groupSide * groupSide);
        down_ = computePass(gpu.device(), downCode, sets, sizeof(std::int32_t), nullptr,
                            groupSide * groupSide);
    }
    ~ScaledInterpolator() override {
        static_cast<void>(gpu_.idle());
    }
    ScaledInterpolator(const ScaledInterpolator&) = delete;
    ScaledInterpolator& operator=(const ScaledInterpolator&) = delete;
    ScaledInterpolator(ScaledInterpolator&&) = delete;
    ScaledInterpolator& operator=(ScaledInterpolator&&) = delete;

    bool begin(std::uint32_t width, std::uint32_t height, SourcePixel pixel) override {
        if (gpu_.idle() != VK_SUCCESS) {
            return false;
        }
        scaled_.clear();
        views_.clear();
        width_ = width;
        height_ = height;
        upKey_ = std::format("scale up {}x{}", width, height);
        downKey_ = std::format("scale down {}x{}", width, height);
        return inner_->begin(width * scale_, height * scale_,
                             SourcePixel{.width = pixel.width * static_cast<float>(scale_),
                                         .height = pixel.height * static_cast<float>(scale_)});
    }

    std::optional<framegen::Submission_t> add(const framegen::GeneratorImage_t& frame) override {
        if (frame.uWidth != width_ || frame.uHeight != height_) {
            return std::nullopt;
        }
        const Scaled* scaled = scaledFor(frame.view);
        const VkDescriptorSet source = views_.set(frame.view);
        if (scaled == nullptr || source == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        before(cmd);
        record(cmd, up_, source, scaled->set, width_ * scale_, height_ * scale_);
        after(cmd);
        gpu_.Submit(cmd);
        report(upKey_, frameBytes(), frameBytes() * scale_ * scale_, width_ * scale_,
               height_ * scale_);
        return inner_->add(scaled->image->view());
    }

    std::optional<framegen::Submission_t> generate(float t,
                                                   const framegen::GeneratorImage_t& output,
                                                   const framegen::OutputHooks_t& hooks) override {
        if (output.uWidth != width_ || output.uHeight != height_) {
            return std::nullopt;
        }
        const Scaled* scaled = scaledFor(output.view);
        const VkDescriptorSet target = views_.set(output.view);
        if (scaled == nullptr || target == VK_NULL_HANDLE ||
            !inner_->generate(t, scaled->image->view(), framegen::OutputHooks_t{})) {
            return std::nullopt;
        }
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        if (hooks.before) {
            hooks.before(cmd);
        }
        before(cmd);
        record(cmd, down_, scaled->set, target, width_, height_);
        if (hooks.after) {
            hooks.after(cmd);
        }
        const std::uint64_t seqNo = gpu_.Submit(cmd);
        report(downKey_, frameBytes(), frameBytes(), width_, height_);
        return framegen::Submission_t{.ulSeqNo = seqNo, .bComplete = true};
    }

  private:
    struct Scaled {
        std::unique_ptr<GeneratorImage> image;
        VkDescriptorSet set{VK_NULL_HANDLE};
    };

    /// The scaled image of the caller's `view`, made the first time it is named.
    const Scaled* scaledFor(VkImageView view) {
        if (const auto known = scaled_.find(view); known != scaled_.end()) {
            return &known->second;
        }
        auto image = std::make_unique<GeneratorImage>(gpu_, width_ * scale_, height_ * scale_);
        const VkDescriptorSet set = views_.set(image->view().view);
        if (set == VK_NULL_HANDLE) {
            return nullptr;
        }
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            return nullptr;
        }
        image->toGeneral(cmd);
        gpu_.Wait(gpu_.Submit(cmd));
        return &scaled_.emplace(view, Scaled{.image = std::move(image), .set = set}).first->second;
    }

    /// Whatever was submitted before, the other generator's work among it, done with the images.
    static void before(VkCommandBuffer cmd) {
        memoryBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                      VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    }
    /// This pass's writes visible to whatever is submitted next.
    static void after(VkCommandBuffer cmd) {
        memoryBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                      VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
    }

    void record(VkCommandBuffer cmd, const Pass& pass, VkDescriptorSet from, VkDescriptorSet to,
                std::uint32_t width, std::uint32_t height) const {
        const std::array<VkDescriptorSet, 2> sets{from, to};
        const auto scale = static_cast<std::int32_t>(scale_);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline.get());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.layout.get(), 0,
                                static_cast<std::uint32_t>(sets.size()), sets.data(), 0, nullptr);
        vkCmdPushConstants(cmd, pass.layout.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(scale),
                           &scale);
        vkCmdDispatch(cmd, groups(width), groups(height), 1);
    }

    [[nodiscard]] std::uint64_t frameBytes() const {
        return std::uint64_t{width_} * height_ * bytesPerPixel;
    }

    void report(const std::string& key, std::uint64_t read, std::uint64_t written,
                std::uint32_t width, std::uint32_t height) const {
        if (GpuSpans* spans = gpu_.spans()) {
            const std::array<NamedWork, 1> parts{
                NamedWork{.name = "pass",
                          .work = GpuWork{.dispatches = 1,
                                          .invocations = std::uint64_t{groups(width)} *
                                                         groups(height) * groupSide * groupSide,
                                          .bytesRead = read,
                                          .bytesWritten = written,
                                          .texelReads = 0,
                                          .sharedReads = 0}}};
            reportWork(*spans, key, parts);
        }
    }

    GeneratorDevice& gpu_;
    std::unique_ptr<Interpolator> inner_;
    std::uint32_t scale_;
    SetLayout image_;
    ViewSets views_;
    Pass up_;
    Pass down_;
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::string upKey_;
    std::string downKey_;
    std::map<VkImageView, Scaled> scaled_;
};

} // namespace

std::unique_ptr<Interpolator> scaledUp(GeneratorDevice& gpu, std::unique_ptr<Interpolator> inner,
                                       std::uint32_t scale, std::string& why) {
    if (inner == nullptr || scale == 0) {
        why = "nothing to scale up";
        return nullptr;
    }
    try {
        return std::make_unique<ScaledInterpolator>(gpu, std::move(inner), scale);
    } catch (const std::runtime_error& failure) {
        why = failure.what();
        return nullptr;
    }
}

} // namespace opensu::frame_gen
