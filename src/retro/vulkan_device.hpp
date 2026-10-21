// vulkan_device — the runner's Vulkan instance (with the validation layer on request) and the one
// device and graphics queue it draws with. A core rendering with Vulkan (phase 2) is handed these.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

#include <vulkan/vulkan.h>

namespace opensu::retro {

/// A Vulkan call or setup step that failed; the message names it.
class VulkanError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

/// Throws VulkanError naming `what` unless `result` is VK_SUCCESS.
void check(VkResult result, const char* what);

/// Whether a debug message is a validation error; the loader's own errors (a driver with no device
/// here) are not.
[[nodiscard]] bool isValidationError(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                     VkDebugUtilsMessageTypeFlagsEXT types);

/// A single-sampled, single-level 2D image description.
[[nodiscard]] VkImageCreateInfo imageInfo(VkFormat format, VkExtent2D extent,
                                          VkImageUsageFlags usage);

class VulkanInstance {
  public:
    /// `extensions` are what the window system needs; empty for offscreen drawing. With
    /// `validation`, VK_LAYER_KHRONOS_validation is enabled when installed, and its errors counted.
    VulkanInstance(std::span<const char* const> extensions, bool validation);
    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;
    VulkanInstance(VulkanInstance&&) = delete;
    VulkanInstance& operator=(VulkanInstance&&) = delete;
    ~VulkanInstance();

    [[nodiscard]] VkInstance handle() const {
        return instance_;
    }
    /// Validation errors reported so far; zero when validation is off.
    [[nodiscard]] std::size_t validationErrors() const {
        return errors_.load();
    }

  private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL onMessage(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types,
        const VkDebugUtilsMessengerCallbackDataEXT* data, void* self);

    VkInstance instance_{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT messenger_{VK_NULL_HANDLE};
    std::atomic<std::size_t> errors_{0};
};

class VulkanDevice {
  public:
    /// The best device with a graphics queue that, given a `surface`, can also present to it.
    VulkanDevice(const VulkanInstance& instance, VkSurfaceKHR surface);
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&) = delete;
    VulkanDevice& operator=(VulkanDevice&&) = delete;
    ~VulkanDevice();

    [[nodiscard]] VkPhysicalDevice physical() const {
        return physical_;
    }
    [[nodiscard]] VkDevice handle() const {
        return device_;
    }
    [[nodiscard]] VkQueue queue() const {
        return queue_;
    }
    [[nodiscard]] std::uint32_t queueFamily() const {
        return queueFamily_;
    }

    /// A memory type allowed by `typeBits` with all of `properties`; throws VulkanError if none.
    [[nodiscard]] std::uint32_t memoryType(std::uint32_t typeBits,
                                           VkMemoryPropertyFlags properties) const;
    /// Memory meeting `needs` with all of `properties`; throws VulkanError.
    [[nodiscard]] VkDeviceMemory allocate(VkMemoryRequirements needs,
                                          VkMemoryPropertyFlags properties) const;
    /// A colour view of the whole of single-level 2D `image`; throws VulkanError.
    [[nodiscard]] VkImageView makeView(VkImage image, VkFormat format) const;

  private:
    VkPhysicalDevice physical_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue queue_{VK_NULL_HANDLE};
    std::uint32_t queueFamily_{0};
};

} // namespace opensu::retro
