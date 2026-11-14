#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "frame_gen/compute_pass.hpp"
#include "frame_gen/interpolator.hpp"
#include "frame_gen/ncnn_model.hpp"
#include "frame_gen/rife_net.hpp"

namespace opensu::frame_gen {
namespace {

constexpr std::uint32_t inCode[] = {
#include "rife_in.comp.inc"
};
constexpr std::uint32_t outCode[] = {
#include "rife_out.comp.inc"
};

using Ivec4 = std::array<std::int32_t, 4>;
using Ivec2 = std::array<std::int32_t, 2>;

/// `shaders/rife_in.comp`'s push constants.
struct InPush {
    Ivec4 dst;
    Ivec2 content;
    std::array<float, 2> pixel;
};
/// `shaders/rife_out.comp`'s push constants.
struct OutPush {
    Ivec4 src;
    Ivec2 size;
    Ivec2 content;
    std::array<float, 2> pixel;
};

constexpr std::uint32_t groupSide = 8;
constexpr std::uint64_t bytesPerPixel = 4;
constexpr std::uint64_t bytesPerFloat = 4;

std::uint32_t groups(std::int32_t pixels) {
    return (static_cast<std::uint32_t>(pixels) + groupSide - 1) / groupSide;
}

std::uint32_t padded(std::uint32_t pixels, std::uint32_t multiple) {
    return (pixels + multiple - 1) / multiple * multiple;
}

/// A part's work: dispatches, their threads and the bytes they read and write.
GpuWork workOf(std::uint64_t dispatches, std::uint64_t invocations, std::uint64_t read,
               std::uint64_t written) {
    return GpuWork{.dispatches = dispatches,
                   .invocations = invocations,
                   .bytesRead = read,
                   .bytesWritten = written,
                   .texelReads = 0,
                   .sharedReads = 0};
}

Ivec4 vec(const RifeNet::Shape& shape) {
    return {shape.offset, shape.width, shape.height, shape.channels};
}

/// Any pass's writes visible to the next pass or copy.
void barrier(VkCommandBuffer cmd) {
    memoryBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                      VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
}

/// RIFE's graph between the last two real frames, each taken at its content's own size: a real
/// frame into `in1` (the one before moved to `in0`), and each frame between run from them at t
/// and written back over the content's pixels (`rife_net`, `shaders/rife_in.comp`,
/// `shaders/rife_out.comp`).
class RifeInterpolator final : public Interpolator {
  public:
    RifeInterpolator(GeneratorDevice& gpu, ncnn::Model model)
        : gpu_{gpu}, net_{gpu, std::move(model)},
          image_{setLayout(gpu.device(), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1)},
          views_{gpu.device(), image_.get()}, in0_{net_.model().blob("in0")},
          in1_{net_.model().blob("in1")}, in2_{net_.model().blob("in2")},
          out0_{net_.model().blob("out0")} {
        const std::array<VkDescriptorSetLayout, 2> sets{net_.setLayout(), image_.get()};
        in_ =
            computePass(gpu.device(), inCode, sets, sizeof(InPush), nullptr, groupSide * groupSide);
        out_ = computePass(gpu.device(), outCode, sets, sizeof(OutPush), nullptr,
                           groupSide * groupSide);
    }
    ~RifeInterpolator() override {
        static_cast<void>(gpu_.idle());
    }
    RifeInterpolator(const RifeInterpolator&) = delete;
    RifeInterpolator& operator=(const RifeInterpolator&) = delete;
    RifeInterpolator(RifeInterpolator&&) = delete;
    RifeInterpolator& operator=(RifeInterpolator&&) = delete;

    bool begin(std::uint32_t width, std::uint32_t height, SourcePixel pixel) override {
        if (gpu_.idle() != VK_SUCCESS || pixel.width < 1.0F || pixel.height < 1.0F) {
            return false;
        }
        const auto contentWidth = std::max<std::uint32_t>(
            1, static_cast<std::uint32_t>(std::lround(static_cast<float>(width) / pixel.width)));
        const auto contentHeight = std::max<std::uint32_t>(
            1, static_cast<std::uint32_t>(std::lround(static_cast<float>(height) / pixel.height)));
        const std::uint32_t multiple = net_.multiple();
        width_ = 0;
        height_ = 0;
        added_ = 0;
        views_.clear();
        try {
            net_.size(padded(contentWidth, multiple), padded(contentHeight, multiple));
        } catch (const std::runtime_error&) {
            return false;
        }
        width_ = width;
        height_ = height;
        content_ = {static_cast<std::int32_t>(contentWidth),
                    static_cast<std::int32_t>(contentHeight)};
        pixel_ = pixel;
        // Named as the other generators name their spans, so runs of each compare.
        ingestKey_ = std::format("ingest {}x{}", width, height);
        generateKey_ = std::format("generate {}x{}", width, height);
        return true;
    }

    std::optional<framegen::Submission_t> add(const framegen::GeneratorImage_t& frame) override {
        if (width_ == 0 || frame.uWidth != width_ || frame.uHeight != height_) {
            return std::nullopt;
        }
        const VkDescriptorSet source = views_.set(frame.view);
        if (source == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        timed_ = gpu_.BeginTiming(cmd);
        barrier(cmd);
        const RifeNet::Shape later = net_.shape(in1_);
        if (added_ > 0) {
            net_.copy(cmd, in1_, in0_);
            const std::uint64_t bytes = blobBytes(later);
            work_.push_back(NamedWork{.name = "copy", .work = workOf(0, 0, bytes, bytes)});
            mark(cmd, "copy");
            barrier(cmd);
        }
        const InPush push{
            .dst = vec(later), .content = content_, .pixel = {pixel_.width, pixel_.height}};
        const std::uint32_t x = groups(later.width);
        const std::uint32_t y = groups(later.height);
        dispatch(cmd, in_, source, push, x, y);
        work_.push_back(NamedWork{.name = "in",
                                  .work = workOf(1, std::uint64_t{x} * y * in_.threads,
                                                 contentBytes(), blobBytes(later))});
        mark(cmd, "in");
        ++added_;
        return framegen::Submission_t{.ulSeqNo = submit(cmd, ingestKey_), .bComplete = true};
    }

    std::optional<framegen::Submission_t> generate(float t,
                                                   const framegen::GeneratorImage_t& output,
                                                   const framegen::OutputHooks_t& hooks) override {
        if (added_ < 2 || output.uWidth != width_ || output.uHeight != height_) {
            return std::nullopt;
        }
        const VkDescriptorSet target = views_.set(output.view);
        if (target == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        timed_ = gpu_.BeginTiming(cmd);
        if (hooks.before) {
            hooks.before(cmd);
        }
        barrier(cmd);
        net_.fill(cmd, in2_, t);
        work_.push_back(
            NamedWork{.name = "time", .work = workOf(0, 0, 0, blobBytes(net_.shape(in2_)))});
        net_.record(cmd, work_);
        mark(cmd, "net");
        const RifeNet::Shape made = net_.shape(out0_);
        const OutPush push{
            .src = vec(made),
            .size = {static_cast<std::int32_t>(width_), static_cast<std::int32_t>(height_)},
            .content = content_,
            .pixel = {pixel_.width, pixel_.height}};
        const std::uint32_t x = groups(static_cast<std::int32_t>(width_));
        const std::uint32_t y = groups(static_cast<std::int32_t>(height_));
        dispatch(cmd, out_, target, push, x, y);
        work_.push_back(NamedWork{.name = "out",
                                  .work = workOf(1, std::uint64_t{x} * y * out_.threads,
                                                 contentBytes() / bytesPerPixel * 3 * bytesPerFloat,
                                                 std::uint64_t{width_} * height_ * bytesPerPixel)});
        mark(cmd, "out");
        if (hooks.after) {
            hooks.after(cmd);
        }
        return framegen::Submission_t{.ulSeqNo = submit(cmd, generateKey_), .bComplete = true};
    }

  private:
    template <typename Push>
    void dispatch(VkCommandBuffer cmd, const Pass& pass, VkDescriptorSet image, const Push& push,
                  std::uint32_t x, std::uint32_t y) const {
        const std::array<VkDescriptorSet, 2> sets{net_.set(), image};
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline.get());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.layout.get(), 0,
                                static_cast<std::uint32_t>(sets.size()), sets.data(), 0, nullptr);
        vkCmdPushConstants(cmd, pass.layout.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push),
                           &push);
        vkCmdDispatch(cmd, x, y, 1);
    }

    [[nodiscard]] static std::uint64_t blobBytes(const RifeNet::Shape& shape) {
        return std::uint64_t{static_cast<std::uint32_t>(shape.width)} *
               static_cast<std::uint32_t>(shape.height) *
               static_cast<std::uint32_t>(shape.channels) * bytesPerFloat;
    }

    /// The bytes of the content's pixels in a caller's frame.
    [[nodiscard]] std::uint64_t contentBytes() const {
        return std::uint64_t{static_cast<std::uint32_t>(content_[0])} *
               static_cast<std::uint32_t>(content_[1]) * bytesPerPixel;
    }

    /// Ends the timed span's part `name` here.
    void mark(VkCommandBuffer cmd, const std::string& name) {
        if (timed_) {
            gpu_.spans()->mark(cmd, *timed_, name);
        }
    }

    /// Submits `cmd`, closing its span when timed, with what each part of it did.
    std::uint64_t submit(VkCommandBuffer cmd, const std::string& key) {
        if (timed_) {
            gpu_.EndTiming(cmd, *timed_, key);
        }
        const std::uint64_t seqNo = gpu_.Submit(cmd);
        if (timed_) {
            gpu_.TimingSubmitted(seqNo);
        }
        timed_.reset();
        if (GpuSpans* spans = gpu_.spans()) {
            reportWork(*spans, key, work_);
        }
        work_.clear();
        return seqNo;
    }

    GeneratorDevice& gpu_;
    RifeNet net_;
    SetLayout image_;
    ViewSets views_;
    int in0_;
    int in1_;
    int in2_;
    int out0_;
    Pass in_;
    Pass out_;
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    Ivec2 content_{0, 0};
    SourcePixel pixel_;
    std::uint64_t added_{0};
    std::string ingestKey_;
    std::string generateKey_;
    std::vector<NamedWork> work_;
    /// The span of the submission being recorded, when timed.
    std::optional<std::uint32_t> timed_;
};

} // namespace

std::unique_ptr<Interpolator> rife(GeneratorDevice& gpu, const std::filesystem::path& model,
                                   std::string& why) {
    try {
        return std::make_unique<RifeInterpolator>(
            gpu, ncnn::load(model / "flownet.param", model / "flownet.bin"));
    } catch (const std::exception& failure) {
        why = std::format("{}: {}", model.string(), failure.what());
        return nullptr;
    }
}

} // namespace opensu::frame_gen
