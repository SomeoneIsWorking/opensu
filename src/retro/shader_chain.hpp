// shader_chain — a libretro slang preset run by librashader on the runner's Vulkan device: the
// preset read and built once, then each frame's passes recorded from the frame texture into the
// target image's placed rectangle. librashader keeps the history, feedback and LUT textures the
// preset asks for. Built with render passes (the device has no dynamic rendering) and one frame in
// flight, as the presenter waits for each frame before the next.
#pragma once

#include <cstddef>
#include <filesystem>

#include <vulkan/vulkan.h>

#include "librashader_library.hpp"
#include "vulkan_device.hpp"

namespace opensu::retro {

/// A Vulkan image with what librashader needs to know of it.
struct ChainImage {
    VkImage image{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkExtent2D extent{};
};

class ShaderChain {
  public:
    /// Loads librashader from `library` and builds `preset` on `device`; throws ShaderError.
    ShaderChain(const std::filesystem::path& library, const std::filesystem::path& preset,
                VkInstance instance, const VulkanDevice& device);
    ShaderChain(const ShaderChain&) = delete;
    ShaderChain& operator=(const ShaderChain&) = delete;
    ShaderChain(ShaderChain&&) = delete;
    ShaderChain& operator=(ShaderChain&&) = delete;
    ~ShaderChain();

    /// Records the passes from `input` (shader-read layout) into `rect` of `output`
    /// (colour-attachment layout, left so); `aspect` is the frame's shape, 0 for its pixels'.
    /// Throws ShaderError.
    void record(VkCommandBuffer commands, const ChainImage& input, const ChainImage& output,
                VkRect2D rect, float aspect);

  private:
    Librashader library_;
    libra_vk_filter_chain_t chain_{nullptr};
    /// Frames drawn, the preset's FrameCount.
    std::size_t frames_{0};
};

} // namespace opensu::retro
