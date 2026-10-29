// streamed_texture — an RGBA image the CPU writes each time it changes, through a staging buffer
// kept mapped, and shaders sample. The frame compositor streams the core's frame through one; the
// overlay pass streams the touch controls through another.
#pragma once

#include <cstdint>
#include <span>

#include <vulkan/vulkan.h>

#include "vulkan_device.hpp"

namespace opensu::retro {

class StreamedTexture {
  public:
    /// The image is sampled and written by transfers, plus whatever `usage` adds.
    StreamedTexture(VulkanDevice& device, VkImageUsageFlags usage);
    StreamedTexture(const StreamedTexture&) = delete;
    StreamedTexture& operator=(const StreamedTexture&) = delete;
    StreamedTexture(StreamedTexture&&) = delete;
    StreamedTexture& operator=(StreamedTexture&&) = delete;
    ~StreamedTexture();

    /// Makes the image `width` by `height` unless it already is, waiting for the GPU first.
    /// Reports whether it was made, which changes `view`. Throws VulkanError.
    bool ensure(unsigned width, unsigned height);

    /// Records copying `rgba` (whole rows of the image's size) into it, left ready to sample.
    void recordUpload(VkCommandBuffer commands, std::span<const std::uint8_t> rgba);
    /// Records moving the image to TRANSFER_DST for a write the caller records, which must be
    /// followed by `endWrite`.
    void beginWrite(VkCommandBuffer commands) const;
    /// Records moving the written image to SHADER_READ_ONLY.
    void endWrite(VkCommandBuffer commands);

    [[nodiscard]] VkImage image() const {
        return image_;
    }
    [[nodiscard]] VkImageView view() const {
        return view_;
    }
    [[nodiscard]] unsigned width() const {
        return width_;
    }
    [[nodiscard]] unsigned height() const {
        return height_;
    }
    /// Whether it holds a written image that can be sampled.
    [[nodiscard]] bool ready() const {
        return ready_;
    }

  private:
    void destroy();

    VulkanDevice& device_;
    VkDevice vk_{VK_NULL_HANDLE};
    VkImageUsageFlags usage_{0};
    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory imageMemory_{VK_NULL_HANDLE};
    VkImageView view_{VK_NULL_HANDLE};
    unsigned width_{0};
    unsigned height_{0};
    bool ready_{false};
    VkBuffer staging_{VK_NULL_HANDLE};
    VkDeviceMemory stagingMemory_{VK_NULL_HANDLE};
    void* stagingMapped_{nullptr};
};

} // namespace opensu::retro
