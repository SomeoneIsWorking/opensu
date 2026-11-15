// compute_pass — the Vulkan pieces a generator's compute passes are made of on a
// `GeneratorDevice`: handles destroyed with their device, descriptor set layouts, compute
// pipelines, the barrier between passes and the report of a submission's work.
#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <utility>

#include "frame_gen/gpu_spans.hpp"

namespace opensu::frame_gen {

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

/// A set of `count` bindings of `type`, numbered from 0, for compute.
[[nodiscard]] SetLayout setLayout(VkDevice device, VkDescriptorType type, std::uint32_t count);

/// A compute pipeline of `code` over `sets` with `pushBytes` of push constants, and the threads
/// of each of its workgroups.
struct Pass {
    PipelineLayout layout;
    Pipeline pipeline;
    std::uint32_t threads{0};
};

/// Throws when the device refuses it.
[[nodiscard]] Pass computePass(VkDevice device, std::span<const std::uint32_t> code,
                               std::span<const VkDescriptorSetLayout> sets, std::uint32_t pushBytes,
                               const VkSpecializationInfo* specialization, std::uint32_t threads);

/// The one-image sets of the views a caller hands a generator in and out, each made the first
/// time it is named.
class ViewSets {
  public:
    /// `layout` holds one storage image.
    ViewSets(VkDevice device, VkDescriptorSetLayout layout);

    /// `view`'s set, or nothing when every set is taken.
    [[nodiscard]] VkDescriptorSet set(VkImageView view);
    /// Every view forgotten, for a new session.
    void clear();

  private:
    static constexpr std::uint32_t capacity = 32;
    VkDevice device_;
    VkDescriptorSetLayout layout_;
    Pool pool_;
    std::map<VkImageView, VkDescriptorSet> sets_;
};

/// Each pass's work made visible to the next.
void computeBarrier(VkCommandBuffer cmd);

/// One recorded part of a submission and what it did.
struct NamedWork {
    std::string name;
    GpuWork work;
};

/// Reports each part of the submission `key` to `spans` as `key name`, and their sum as `key`.
void reportWork(GpuSpans& spans, const std::string& key, std::span<const NamedWork> parts);

} // namespace opensu::frame_gen
