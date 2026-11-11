// generator_gpu — the Vulkan device frame generation runs on, outside any window: an instance, the
// first GPU with what the generators need (a discrete one before others), a compute queue whose
// submissions a timeline semaphore numbers (`framegen::IQueue`), RGBA8 images in GENERAL layout
// and host-mapped buffers to upload real frames and read generated ones back.
#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <utility>

#include "framegen/Device.h"
#include "framegen/LosslessGenerator.h"

namespace opensu::frame_gen {

/// Throws `std::runtime_error` naming `what` unless `result` is VK_SUCCESS.
void check(VkResult result, const char* what);

class GeneratorDevice final : public framegen::IQueue {
  public:
    /// Throws when no GPU has robustness2's null descriptors, timeline semaphores, storage images
    /// of any format and a compute queue.
    GeneratorDevice();
    ~GeneratorDevice() override;
    GeneratorDevice(const GeneratorDevice&) = delete;
    GeneratorDevice& operator=(const GeneratorDevice&) = delete;
    GeneratorDevice(GeneratorDevice&&) = delete;
    GeneratorDevice& operator=(GeneratorDevice&&) = delete;

    /// The device as the framegen library takes it, its lines logged through Lucent.
    [[nodiscard]] framegen::DeviceDesc_t desc();
    [[nodiscard]] VkDevice device() const noexcept {
        return device_;
    }
    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    /// Memory of `properties` for `requirements`, bound by the caller.
    [[nodiscard]] VkDeviceMemory allocate(const VkMemoryRequirements& requirements,
                                          VkMemoryPropertyFlags properties) const;

    VkCommandBuffer Begin() override;
    std::uint64_t Submit(VkCommandBuffer cmd) override;
    std::uint64_t Completed() override;
    void Wait(std::uint64_t seqNo) override;

    /// Waits for everything submitted.
    [[nodiscard]] VkResult idle() noexcept;

  private:
    void choose();
    std::string lacks(VkPhysicalDevice candidate, const VkPhysicalDeviceProperties& properties);
    void makeDevice();
    void release() noexcept;

    VkInstance instance_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_{VK_NULL_HANDLE};
    std::string name_;
    std::uint32_t family_{0};
    bool float16_{false};
    bool scalarLayout_{false};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue queue_{VK_NULL_HANDLE};
    VkCommandPool pool_{VK_NULL_HANDLE};
    VkSemaphore timeline_{VK_NULL_HANDLE};
    std::uint64_t submittedSeq_{0};
    /// Command buffers submitted, oldest first, each with its submission's number.
    std::deque<std::pair<std::uint64_t, VkCommandBuffer>> submitted_;
};

/// An RGBA8 image a generator reads or writes, in GENERAL layout from its first use.
class GeneratorImage {
  public:
    GeneratorImage(const GeneratorDevice& gpu, std::uint32_t width, std::uint32_t height);
    ~GeneratorImage();
    GeneratorImage(const GeneratorImage&) = delete;
    GeneratorImage& operator=(const GeneratorImage&) = delete;
    GeneratorImage(GeneratorImage&&) = delete;
    GeneratorImage& operator=(GeneratorImage&&) = delete;

    [[nodiscard]] framegen::GeneratorImage_t view() const noexcept {
        return framegen::GeneratorImage_t{.view = view_, .uWidth = width_, .uHeight = height_};
    }
    [[nodiscard]] VkImage image() const noexcept {
        return image_;
    }

    /// Records the move into GENERAL, the contents left undefined.
    void toGeneral(VkCommandBuffer cmd) const;

  private:
    VkDevice device_;
    std::uint32_t width_;
    std::uint32_t height_;
    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkImageView view_{VK_NULL_HANDLE};
};

/// A buffer the CPU writes a real frame into or reads a generated one from, mapped for its life.
class HostBuffer {
  public:
    HostBuffer(const GeneratorDevice& gpu, VkDeviceSize bytes, VkBufferUsageFlags usage);
    ~HostBuffer();
    HostBuffer(const HostBuffer&) = delete;
    HostBuffer& operator=(const HostBuffer&) = delete;
    HostBuffer(HostBuffer&&) = delete;
    HostBuffer& operator=(HostBuffer&&) = delete;

    [[nodiscard]] VkBuffer buffer() const noexcept {
        return buffer_;
    }
    [[nodiscard]] std::span<std::uint8_t> bytes() const noexcept {
        return {static_cast<std::uint8_t*>(mapped_), static_cast<std::size_t>(bytes_)};
    }

  private:
    VkDevice device_;
    VkDeviceSize bytes_;
    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    void* mapped_{nullptr};
};

/// A copy of a whole `width` by `height` image to or from a tightly packed buffer.
[[nodiscard]] VkBufferImageCopy wholeImage(std::uint32_t width, std::uint32_t height);

/// Makes `written` at `from` visible to `read` at `to`.
void memoryBarrier(VkCommandBuffer cmd, VkPipelineStageFlags from, VkAccessFlags written,
                   VkPipelineStageFlags to, VkAccessFlags read);

} // namespace opensu::frame_gen
