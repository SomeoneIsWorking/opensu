#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "frame_gen/interpolator.hpp"

namespace opensu::frame_gen {
namespace {

constexpr std::uint32_t takeCode[] = {
#include "flow_take.comp.inc"
};
constexpr std::uint32_t reduceCode[] = {
#include "flow_reduce.comp.inc"
};
constexpr std::uint32_t searchCode[] = {
#include "flow_search.comp.inc"
};
constexpr std::uint32_t chooseCode[] = {
#include "flow_choose.comp.inc"
};
constexpr std::uint32_t makeCode[] = {
#include "flow_make.comp.inc"
};

/// The shaders' push constants (`flow_push.glsl`).
struct FlowPush {
    std::int32_t width;
    std::int32_t height;
    std::int32_t blocksX;
    std::int32_t blocksY;
    std::int32_t range;
    std::int32_t hasParent;
    float t;
    float pixelWidth;
    float pixelHeight;
    std::int32_t scale;
};

/// Pyramid levels: the frame and three halvings, so the coarsest search's radius reaches
/// `coarsestRange` << 3 pixels of the frame.
constexpr std::size_t levels = 4;
/// At most 8: the search's shared tiles hold the windows of vectors that long (`flow_search.comp`).
constexpr std::int32_t coarsestRange = 8;
constexpr std::uint32_t blockSize = flowBlockPixels;
/// The descriptor set every pass's read counts are in (`flow_push.glsl`).
constexpr std::uint32_t countSet = 3;

/// The read counts' buffer (`Reads` in `flow_push.glsl`).
struct Reads {
    std::uint32_t texels;
    std::uint32_t shared;
};
constexpr std::uint32_t groupSize = 8;
/// Descriptor sets for the caller's views (frames in, frames out) a session can name.
constexpr std::uint32_t viewSets = 32;

std::uint32_t groups(std::uint32_t pixels) {
    return (pixels + groupSize - 1) / groupSize;
}

std::uint32_t half(std::uint32_t pixels) {
    return (pixels + 1) / 2;
}

/// A Vulkan handle destroyed with its device.
template <typename Handle, void (*destroy)(VkDevice, Handle, const VkAllocationCallbacks*)>
class Owned {
  public:
    Owned() = default;
    Owned(VkDevice device, Handle handle) : device_{device}, handle_{handle} {
    }
    ~Owned() {
        if (handle_ != VK_NULL_HANDLE) {
            destroy(device_, handle_, nullptr);
        }
    }
    Owned(const Owned&) = delete;
    Owned& operator=(const Owned&) = delete;
    Owned(Owned&& other) noexcept
        : device_{other.device_}, handle_{std::exchange(other.handle_, VK_NULL_HANDLE)} {
    }
    Owned& operator=(Owned&& other) noexcept {
        std::swap(device_, other.device_);
        std::swap(handle_, other.handle_);
        return *this;
    }
    [[nodiscard]] Handle get() const noexcept {
        return handle_;
    }

  private:
    VkDevice device_{VK_NULL_HANDLE};
    Handle handle_{VK_NULL_HANDLE};
};

using SetLayout = Owned<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>;
using PipelineLayout = Owned<VkPipelineLayout, vkDestroyPipelineLayout>;
using Pipeline = Owned<VkPipeline, vkDestroyPipeline>;
using Pool = Owned<VkDescriptorPool, vkDestroyDescriptorPool>;

SetLayout storageImages(VkDevice device, std::uint32_t count) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        bindings.push_back(
            VkDescriptorSetLayoutBinding{.binding = i,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                         .descriptorCount = 1,
                                         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                         .pImmutableSamplers = nullptr});
    }
    const VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = count,
        .pBindings = bindings.data()};
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    check(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout),
          "vkCreateDescriptorSetLayout");
    return SetLayout{device, layout};
}

/// A compute pipeline of `code` over the sets `sets`, with the shared push constants.
struct Pass {
    PipelineLayout layout;
    Pipeline pipeline;
};

/// The texel count's set: one storage buffer.
SetLayout countLayout(VkDevice device) {
    const VkDescriptorSetLayoutBinding binding{.binding = 0,
                                               .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                               .descriptorCount = 1,
                                               .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                               .pImmutableSamplers = nullptr};
    const VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &binding};
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    check(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout),
          "vkCreateDescriptorSetLayout");
    return SetLayout{device, layout};
}

/// `own` sets, the sets up to `countSet` empty, then the count's; texels counted when `counting`.
Pass passOf(VkDevice device, std::span<const std::uint32_t> code,
            std::span<const VkDescriptorSetLayout> own, VkDescriptorSetLayout empty,
            VkDescriptorSetLayout count, bool counting) {
    std::vector<VkDescriptorSetLayout> sets(own.begin(), own.end());
    sets.resize(countSet, empty);
    sets.push_back(count);
    const VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = sizeof(FlowPush)};
    const VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<std::uint32_t>(sets.size()),
        .pSetLayouts = sets.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push};
    VkPipelineLayout layout = VK_NULL_HANDLE;
    check(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout");
    Pass pass{.layout = PipelineLayout{device, layout}, .pipeline = {}};
    const VkShaderModuleCreateInfo moduleInfo{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = 0,
                                              .codeSize = code.size_bytes(),
                                              .pCode = code.data()};
    VkShaderModule module = VK_NULL_HANDLE;
    check(vkCreateShaderModule(device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
    const VkBool32 countReads = counting ? VK_TRUE : VK_FALSE;
    const VkSpecializationMapEntry entry{.constantID = 0, .offset = 0, .size = sizeof(countReads)};
    const VkSpecializationInfo specialization{.mapEntryCount = 1,
                                              .pMapEntries = &entry,
                                              .dataSize = sizeof(countReads),
                                              .pData = &countReads};
    const VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .pNext = nullptr,
                  .flags = 0,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = module,
                  .pName = "main",
                  .pSpecializationInfo = &specialization},
        .layout = layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1};
    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult made =
        vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    vkDestroyShaderModule(device, module, nullptr);
    check(made, "vkCreateComputePipelines");
    pass.pipeline = Pipeline{device, pipeline};
    return pass;
}

/// Each pass's work made visible to the next.
void computeBarrier(VkCommandBuffer cmd) {
    memoryBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
}

/// openSU's own generator: block motion between the two real frames, searched coarse to fine on
/// a luma pyramid when a real frame comes, and each frame between made from it by the pixels that
/// agree best, the real pixels no motion takes shown in place (`shaders/flow_*.comp`).
class FlowInterpolator final : public Interpolator {
  public:
    FlowInterpolator(GeneratorDevice& gpu, std::uint32_t scale)
        : gpu_{gpu}, device_{gpu.device()}, scale_{scale}, one_{storageImages(device_, 1)},
          two_{storageImages(device_, 2)}, three_{storageImages(device_, 3)},
          four_{storageImages(device_, 4)}, empty_{storageImages(device_, 0)},
          countLayout_{countLayout(device_)}, counting_{gpu.spans() != nullptr},
          texels_{gpu, sizeof(Reads), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT} {
        const auto pass = [this](std::span<const std::uint32_t> code,
                                 std::span<const VkDescriptorSetLayout> own) {
            return passOf(device_, code, own, empty_.get(), countLayout_.get(), counting_);
        };
        const std::array<VkDescriptorSetLayout, 2> takeSets{one_.get(), two_.get()};
        take_ = pass(takeCode, takeSets);
        const std::array<VkDescriptorSetLayout, 1> reduceSets{two_.get()};
        reduce_ = pass(reduceCode, reduceSets);
        const std::array<VkDescriptorSetLayout, 1> searchSets{four_.get()};
        search_ = pass(searchCode, searchSets);
        const std::array<VkDescriptorSetLayout, 2> chooseSets{three_.get(), two_.get()};
        choose_ = pass(chooseCode, chooseSets);
        const std::array<VkDescriptorSetLayout, 2> makeSets{four_.get(), one_.get()};
        make_ = pass(makeCode, makeSets);
        makeCountSet();
    }
    ~FlowInterpolator() override {
        static_cast<void>(gpu_.idle());
    }
    FlowInterpolator(const FlowInterpolator&) = delete;
    FlowInterpolator& operator=(const FlowInterpolator&) = delete;
    FlowInterpolator(FlowInterpolator&&) = delete;
    FlowInterpolator& operator=(FlowInterpolator&&) = delete;

    [[nodiscard]] std::uint32_t firstOutputLead() const noexcept override {
        return flowFirstOutputLead;
    }

    bool begin(std::uint32_t width, std::uint32_t height, SourcePixel pixel) override {
        if (gpu_.idle() != VK_SUCCESS || pixel.width < 1.0F || pixel.height < 1.0F) {
            return false;
        }
        pixel_ = SourcePixel{.width = pixel.width * static_cast<float>(scale_),
                             .height = pixel.height * static_cast<float>(scale_)};
        try {
            make(width * scale_, height * scale_);
        } catch (const std::runtime_error&) {
            width_ = 0;
            return false;
        }
        return true;
    }

    std::optional<framegen::Submission_t> add(const framegen::GeneratorImage_t& frame) override {
        if (width_ == 0 || frame.uWidth * scale_ != width_ || frame.uHeight * scale_ != height_) {
            return std::nullopt;
        }
        const std::size_t slot = added_ % 2;
        const VkDescriptorSet source = viewSet(frame.view);
        if (source == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        const std::optional<std::uint32_t> timed = gpu_.BeginTiming(cmd);
        memoryBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        const std::array<VkDescriptorSet, 2> takeSets{source, takeSets_[slot]};
        dispatch(
            cmd, take_, takeSets, pushOf(width_, height_), groups(width_), groups(height_),
            {.read = frameBytes(), .written = frames_[slot]->bytes() + lumas_[slot][0]->bytes()});
        std::uint32_t levelWidth = width_;
        std::uint32_t levelHeight = height_;
        for (std::size_t level = 1; level < levels; ++level) {
            levelWidth = half(levelWidth);
            levelHeight = half(levelHeight);
            computeBarrier(cmd);
            const std::array<VkDescriptorSet, 1> sets{reduceSets_[slot][level]};
            dispatch(cmd, reduce_, sets, pushOf(levelWidth, levelHeight), groups(levelWidth),
                     groups(levelHeight),
                     {.read = lumas_[slot][level - 1]->bytes(),
                      .written = lumas_[slot][level]->bytes()});
        }
        if (added_ >= 1) {
            for (std::size_t level = levels; level-- > 0;) {
                computeBarrier(cmd);
                const auto [w, h] = sizes_[level];
                FlowPush push = pushOf(w, h);
                push.hasParent = level + 1 < levels ? 1 : 0;
                // The search matches in the level's own pixels.
                push.pixelWidth = 1.0F;
                push.pixelHeight = 1.0F;
                const std::array<VkDescriptorSet, 1> sets{searchSets_[slot][level]};
                const std::uint64_t parent = push.hasParent != 0 ? fields_[level + 1]->bytes() : 0;
                // One workgroup per block.
                dispatch(cmd, search_, sets, push, static_cast<std::uint32_t>(push.blocksX),
                         static_cast<std::uint32_t>(push.blocksY),
                         {.read = (2 * lumas_[slot][level]->bytes()) + parent,
                          .written = fields_[level]->bytes()});
            }
        }
        ++added_;
        return framegen::Submission_t{.ulSeqNo = submit(cmd, timed, ingestKey_), .bComplete = true};
    }

    std::optional<framegen::Submission_t> generate(float t,
                                                   const framegen::GeneratorImage_t& output,
                                                   const framegen::OutputHooks_t& hooks) override {
        if (added_ < 2 || output.uWidth * scale_ != width_ || output.uHeight * scale_ != height_) {
            return std::nullopt;
        }
        const VkDescriptorSet out = viewSet(output.view);
        if (out == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            return std::nullopt;
        }
        const std::optional<std::uint32_t> timed = gpu_.BeginTiming(cmd);
        if (hooks.before) {
            hooks.before(cmd);
        }
        memoryBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        FlowPush push = pushOf(width_, height_);
        push.t = t;
        const std::size_t slot = (added_ - 1) % 2;
        const VkClearColorValue none{.uint32 = {0, 0, 0, 0}};
        const VkImageSubresourceRange whole{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                            .baseMipLevel = 0,
                                            .levelCount = 1,
                                            .baseArrayLayer = 0,
                                            .layerCount = 1};
        vkCmdClearColorImage(cmd, used_->image(), VK_IMAGE_LAYOUT_GENERAL, &none, 1, &whole);
        work_.bytesWritten += used_->bytes();
        memoryBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        const std::array<VkDescriptorSet, 2> chooseSets{chooseSets_[slot], choiceSet_};
        const std::uint64_t frame = frames_[0]->bytes();
        // The used marks are read and written.
        dispatch(cmd, choose_, chooseSets, push, groups(width_), groups(height_),
                 {.read = (2 * frame) + fields_[0]->bytes() + used_->bytes(),
                  .written = choice_->bytes() + used_->bytes()});
        computeBarrier(cmd);
        const std::array<VkDescriptorSet, 2> makeSets{makeSets_[slot], out};
        // Only the centres of the scaled pixels are made.
        dispatch(
            cmd, make_, makeSets, push, groups(width_ / scale_), groups(height_ / scale_),
            {.read = (2 * frame) + choice_->bytes() + used_->bytes(), .written = frameBytes()});
        if (hooks.after) {
            hooks.after(cmd);
        }
        return framegen::Submission_t{.ulSeqNo = submit(cmd, timed, generateKey_),
                                      .bComplete = true};
    }

  private:
    [[nodiscard]] FlowPush pushOf(std::uint32_t width, std::uint32_t height) const {
        return FlowPush{.width = static_cast<std::int32_t>(width),
                        .height = static_cast<std::int32_t>(height),
                        .blocksX = static_cast<std::int32_t>((width + blockSize - 1) / blockSize),
                        .blocksY = static_cast<std::int32_t>((height + blockSize - 1) / blockSize),
                        .range = coarsestRange,
                        .hasParent = 0,
                        .t = 0.5F,
                        .pixelWidth = pixel_.width,
                        .pixelHeight = pixel_.height,
                        .scale = static_cast<std::int32_t>(scale_)};
    }

    /// The bytes of a real or made frame as the caller's images hold it.
    [[nodiscard]] std::uint64_t frameBytes() const {
        constexpr std::uint64_t bytesPerPixel = 4;
        return std::uint64_t{width_ / scale_} * (height_ / scale_) * bytesPerPixel;
    }

    /// Submits `cmd`, closing its span when timed, with what it dispatched; while measuring, waits
    /// for it to read the texels it counted.
    std::uint64_t submit(VkCommandBuffer cmd, std::optional<std::uint32_t> timed,
                         const std::string& key) {
        if (timed) {
            gpu_.EndTiming(cmd, *timed, key);
        }
        const std::uint64_t seqNo = gpu_.Submit(cmd);
        if (timed) {
            gpu_.TimingSubmitted(seqNo);
        }
        if (counting_) {
            gpu_.Wait(seqNo);
            Reads reads{};
            std::memcpy(&reads, texels_.bytes().data(), sizeof(reads));
            work_.texelReads = reads.texels;
            work_.sharedReads = reads.shared;
            std::memset(texels_.bytes().data(), 0, sizeof(reads));
        }
        if (GpuSpans* spans = gpu_.spans()) {
            spans->work(key, work_);
        }
        work_ = {};
        return seqNo;
    }

    /// The bytes of images a dispatch reads and writes, each image once.
    struct ImageBytes {
        std::uint64_t read;
        std::uint64_t written;
    };

    void dispatch(VkCommandBuffer cmd, const Pass& pass, std::span<const VkDescriptorSet> sets,
                  const FlowPush& push, std::uint32_t x, std::uint32_t y, ImageBytes bytes) {
        ++work_.dispatches;
        work_.invocations += std::uint64_t{x} * y * groupSize * groupSize;
        work_.bytesRead += bytes.read;
        work_.bytesWritten += bytes.written;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline.get());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.layout.get(), 0,
                                static_cast<std::uint32_t>(sets.size()), sets.data(), 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.layout.get(), countSet, 1,
                                &countSet_, 0, nullptr);
        vkCmdPushConstants(cmd, pass.layout.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push),
                           &push);
        vkCmdDispatch(cmd, x, y, 1);
    }

    [[nodiscard]] VkDescriptorSet allocate(VkDescriptorSetLayout layout) const {
        const VkDescriptorSetAllocateInfo info{.sType =
                                                   VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                               .pNext = nullptr,
                                               .descriptorPool = pool_.get(),
                                               .descriptorSetCount = 1,
                                               .pSetLayouts = &layout};
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(device_, &info, &set) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
        return set;
    }

    /// Writes `views` into `set`'s bindings in order; a null view is robustness2's null image.
    void write(VkDescriptorSet set, std::span<const VkImageView> views) const {
        std::vector<VkDescriptorImageInfo> images;
        images.reserve(views.size());
        for (VkImageView view : views) {
            images.push_back(VkDescriptorImageInfo{.sampler = VK_NULL_HANDLE,
                                                   .imageView = view,
                                                   .imageLayout = VK_IMAGE_LAYOUT_GENERAL});
        }
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(images.size());
        for (std::size_t i = 0; i < images.size(); ++i) {
            writes.push_back(
                VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .pNext = nullptr,
                                     .dstSet = set,
                                     .dstBinding = static_cast<std::uint32_t>(i),
                                     .dstArrayElement = 0,
                                     .descriptorCount = 1,
                                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                     .pImageInfo = &images[i],
                                     .pBufferInfo = nullptr,
                                     .pTexelBufferView = nullptr});
        }
        vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                               nullptr);
    }

    [[nodiscard]] VkDescriptorSet fixedSet(VkDescriptorSetLayout layout,
                                           std::span<const VkImageView> views) const {
        const VkDescriptorSet set = allocate(layout);
        if (set == VK_NULL_HANDLE) {
            throw std::runtime_error{"no descriptor set for the flow passes"};
        }
        write(set, views);
        return set;
    }

    /// The one-image set of a view the caller hands in or out, made the first time it is named.
    VkDescriptorSet viewSet(VkImageView view) {
        if (const auto known = views_.find(view); known != views_.end()) {
            return known->second;
        }
        if (views_.size() >= viewSets) {
            return VK_NULL_HANDLE;
        }
        const VkDescriptorSet set = allocate(one_.get());
        if (set != VK_NULL_HANDLE) {
            const std::array<VkImageView, 1> one{view};
            write(set, one);
            views_.emplace(view, set);
        }
        return set;
    }

    /// The texel count's buffer, zeroed, in its own set.
    void makeCountSet() {
        const VkDescriptorPoolSize size{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        .descriptorCount = 1};
        const VkDescriptorPoolCreateInfo poolInfo{.sType =
                                                      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                  .pNext = nullptr,
                                                  .flags = 0,
                                                  .maxSets = 1,
                                                  .poolSizeCount = 1,
                                                  .pPoolSizes = &size};
        VkDescriptorPool pool = VK_NULL_HANDLE;
        check(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
        countPool_ = Pool{device_, pool};
        const VkDescriptorSetLayout layout = countLayout_.get();
        const VkDescriptorSetAllocateInfo info{.sType =
                                                   VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                               .pNext = nullptr,
                                               .descriptorPool = pool,
                                               .descriptorSetCount = 1,
                                               .pSetLayouts = &layout};
        check(vkAllocateDescriptorSets(device_, &info, &countSet_), "vkAllocateDescriptorSets");
        const VkDescriptorBufferInfo buffer{
            .buffer = texels_.buffer(), .offset = 0, .range = sizeof(Reads)};
        const VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                         .pNext = nullptr,
                                         .dstSet = countSet_,
                                         .dstBinding = 0,
                                         .dstArrayElement = 0,
                                         .descriptorCount = 1,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         .pImageInfo = nullptr,
                                         .pBufferInfo = &buffer,
                                         .pTexelBufferView = nullptr};
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
        std::memset(texels_.bytes().data(), 0, sizeof(Reads));
    }

    [[nodiscard]] static VkImageView viewOf(const std::unique_ptr<GeneratorImage>& image) {
        return image->view().view;
    }

    void make(std::uint32_t width, std::uint32_t height) {
        views_.clear();
        frames_ = {};
        lumas_ = {};
        fields_ = {};
        choice_.reset();
        used_.reset();
        pool_ = Pool{};
        width_ = 0;
        height_ = 0;
        added_ = 0;
        std::uint32_t w = width;
        std::uint32_t h = height;
        for (std::size_t level = 0; level < levels; ++level) {
            sizes_[level] = {w, h};
            const std::uint32_t bx = (w + blockSize - 1) / blockSize;
            const std::uint32_t by = (h + blockSize - 1) / blockSize;
            fields_[level] =
                std::make_unique<GeneratorImage>(gpu_, bx, by, VK_FORMAT_R32G32_SFLOAT);
            for (std::size_t slot = 0; slot < 2; ++slot) {
                lumas_[slot][level] =
                    std::make_unique<GeneratorImage>(gpu_, w, h, VK_FORMAT_R32_SFLOAT);
            }
            w = half(w);
            h = half(h);
        }
        for (auto& frame : frames_) {
            frame = std::make_unique<GeneratorImage>(gpu_, width, height);
        }
        choice_ =
            std::make_unique<GeneratorImage>(gpu_, width, height, VK_FORMAT_R16G16B16A16_SFLOAT);
        used_ = std::make_unique<GeneratorImage>(gpu_, width, height, VK_FORMAT_R32_UINT);
        const std::uint32_t fixedSets = 2 + 2 * levels + 2 * levels + 2 + 2 + 1;
        const VkDescriptorPoolSize size{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                        .descriptorCount = (fixedSets + viewSets) * 4};
        const VkDescriptorPoolCreateInfo poolInfo{.sType =
                                                      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                  .pNext = nullptr,
                                                  .flags = 0,
                                                  .maxSets = fixedSets + viewSets,
                                                  .poolSizeCount = 1,
                                                  .pPoolSizes = &size};
        VkDescriptorPool pool = VK_NULL_HANDLE;
        check(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
        pool_ = Pool{device_, pool};
        for (std::size_t slot = 0; slot < 2; ++slot) {
            const std::size_t other = 1 - slot;
            const std::array<VkImageView, 2> take{viewOf(frames_[slot]), viewOf(lumas_[slot][0])};
            takeSets_[slot] = fixedSet(two_.get(), take);
            for (std::size_t level = 1; level < levels; ++level) {
                const std::array<VkImageView, 2> reduce{viewOf(lumas_[slot][level - 1]),
                                                        viewOf(lumas_[slot][level])};
                reduceSets_[slot][level] = fixedSet(two_.get(), reduce);
            }
            // With `slot` the later real frame, the other the earlier.
            for (std::size_t level = 0; level < levels; ++level) {
                const VkImageView parent =
                    level + 1 < levels ? viewOf(fields_[level + 1]) : VK_NULL_HANDLE;
                const std::array<VkImageView, 4> search{viewOf(lumas_[other][level]),
                                                        viewOf(lumas_[slot][level]), parent,
                                                        viewOf(fields_[level])};
                searchSets_[slot][level] = fixedSet(four_.get(), search);
            }
            const std::array<VkImageView, 3> chose{viewOf(frames_[other]), viewOf(frames_[slot]),
                                                   viewOf(fields_[0])};
            chooseSets_[slot] = fixedSet(three_.get(), chose);
            const std::array<VkImageView, 4> made{viewOf(frames_[other]), viewOf(frames_[slot]),
                                                  viewOf(choice_), viewOf(used_)};
            makeSets_[slot] = fixedSet(four_.get(), made);
        }
        const std::array<VkImageView, 2> choice{viewOf(choice_), viewOf(used_)};
        choiceSet_ = fixedSet(two_.get(), choice);
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            throw std::runtime_error{"no command buffer for the flow images"};
        }
        for (const auto& frame : frames_) {
            frame->toGeneral(cmd);
        }
        for (const auto& slot : lumas_) {
            for (const auto& luma : slot) {
                luma->toGeneral(cmd);
            }
        }
        for (const auto& field : fields_) {
            field->toGeneral(cmd);
        }
        choice_->toGeneral(cmd);
        used_->toGeneral(cmd);
        gpu_.Wait(gpu_.Submit(cmd));
        width_ = width;
        height_ = height;
        // Named as the Lossless generator names its spans, so runs of both compare.
        ingestKey_ = std::format("ingest {}x{}", width / scale_, height / scale_);
        generateKey_ = std::format("generate {}x{}", width / scale_, height / scale_);
    }

    GeneratorDevice& gpu_;
    VkDevice device_;
    /// The frame's pixel in the pixels flow works on.
    std::uint32_t scale_;
    SetLayout one_;
    SetLayout two_;
    SetLayout three_;
    SetLayout four_;
    SetLayout empty_;
    SetLayout countLayout_;
    bool counting_;
    HostBuffer texels_;
    Pool countPool_;
    VkDescriptorSet countSet_{VK_NULL_HANDLE};
    Pass take_;
    Pass reduce_;
    Pass search_;
    Pass choose_;
    Pass make_;
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint64_t added_{0};
    SourcePixel pixel_;
    std::string ingestKey_;
    std::string generateKey_;
    GpuWork work_;
    std::array<std::pair<std::uint32_t, std::uint32_t>, levels> sizes_{};
    std::array<std::unique_ptr<GeneratorImage>, 2> frames_;
    std::array<std::array<std::unique_ptr<GeneratorImage>, levels>, 2> lumas_;
    std::array<std::unique_ptr<GeneratorImage>, levels> fields_;
    /// Each pixel's chosen motion and how badly it matched, for the frame being made.
    std::unique_ptr<GeneratorImage> choice_;
    /// Which real pixels the chosen motion takes: bit 0 the earlier frame's, bit 1 the later's.
    std::unique_ptr<GeneratorImage> used_;
    Pool pool_;
    std::array<VkDescriptorSet, 2> takeSets_{};
    std::array<std::array<VkDescriptorSet, levels>, 2> reduceSets_{};
    std::array<std::array<VkDescriptorSet, levels>, 2> searchSets_{};
    std::array<VkDescriptorSet, 2> chooseSets_{};
    VkDescriptorSet choiceSet_{VK_NULL_HANDLE};
    std::array<VkDescriptorSet, 2> makeSets_{};
    std::map<VkImageView, VkDescriptorSet> views_;
};

} // namespace

std::unique_ptr<Interpolator> flow(GeneratorDevice& gpu, std::uint32_t scale, std::string& why) {
    if (scale == 0) {
        why = "flow's scale is 0";
        return nullptr;
    }
    try {
        return std::make_unique<FlowInterpolator>(gpu, scale);
    } catch (const std::runtime_error& failure) {
        why = failure.what();
        return nullptr;
    }
}

} // namespace opensu::frame_gen
