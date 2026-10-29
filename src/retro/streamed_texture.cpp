#include "streamed_texture.hpp"

#include <algorithm>
#include <cstring>

namespace opensu::retro {

StreamedTexture::StreamedTexture(VulkanDevice& device, VkImageUsageFlags usage)
    : device_{device}, vk_{device.handle()},
      usage_{usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT} {
}

StreamedTexture::~StreamedTexture() {
    destroy();
}

bool StreamedTexture::ensure(unsigned width, unsigned height) {
    if (image_ != VK_NULL_HANDLE && width == width_ && height == height_) {
        return false;
    }
    check(device_.waitIdle(), "vkDeviceWaitIdle");
    destroy();
    try {
        const VkImageCreateInfo create =
            imageInfo(VK_FORMAT_R8G8B8A8_UNORM, {width, height}, usage_);
        check(vkCreateImage(vk_, &create, nullptr, &image_), "vkCreateImage");
        VkMemoryRequirements needs{};
        vkGetImageMemoryRequirements(vk_, image_, &needs);
        imageMemory_ = device_.allocate(needs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkBindImageMemory(vk_, image_, imageMemory_, 0), "vkBindImageMemory");
        view_ = device_.makeView(image_, VK_FORMAT_R8G8B8A8_UNORM);

        VkBufferCreateInfo buffer{};
        buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer.size = static_cast<VkDeviceSize>(width) * height * 4;
        buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(vk_, &buffer, nullptr, &staging_), "vkCreateBuffer");
        vkGetBufferMemoryRequirements(vk_, staging_, &needs);
        stagingMemory_ = device_.allocate(needs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkBindBufferMemory(vk_, staging_, stagingMemory_, 0), "vkBindBufferMemory");
        check(vkMapMemory(vk_, stagingMemory_, 0, VK_WHOLE_SIZE, 0, &stagingMapped_),
              "vkMapMemory");
    } catch (...) {
        destroy();
        throw;
    }
    width_ = width;
    height_ = height;
    return true;
}

void StreamedTexture::destroy() {
    if (stagingMapped_ != nullptr) {
        vkUnmapMemory(vk_, stagingMemory_);
        stagingMapped_ = nullptr;
    }
    vkDestroyBuffer(vk_, staging_, nullptr);
    vkFreeMemory(vk_, stagingMemory_, nullptr);
    vkDestroyImageView(vk_, view_, nullptr);
    vkDestroyImage(vk_, image_, nullptr);
    vkFreeMemory(vk_, imageMemory_, nullptr);
    staging_ = VK_NULL_HANDLE;
    stagingMemory_ = VK_NULL_HANDLE;
    view_ = VK_NULL_HANDLE;
    image_ = VK_NULL_HANDLE;
    imageMemory_ = VK_NULL_HANDLE;
    width_ = 0;
    height_ = 0;
    ready_ = false;
}

void StreamedTexture::beginWrite(VkCommandBuffer commands) const {
    imageBarrier(commands, image_,
                 ready_ ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 ready_ ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                 ready_ ? VK_ACCESS_SHADER_READ_BIT : 0, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_ACCESS_TRANSFER_WRITE_BIT);
}

void StreamedTexture::endWrite(VkCommandBuffer commands) {
    imageBarrier(commands, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_SHADER_READ_BIT);
    ready_ = true;
}

void StreamedTexture::recordUpload(VkCommandBuffer commands, std::span<const std::uint8_t> rgba) {
    const std::size_t size = static_cast<std::size_t>(width_) * height_ * 4;
    std::memcpy(stagingMapped_, rgba.data(), std::min(size, rgba.size()));
    beginWrite(commands);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width_, height_, 1};
    vkCmdCopyBufferToImage(commands, staging_, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);
    endWrite(commands);
}

} // namespace opensu::retro
