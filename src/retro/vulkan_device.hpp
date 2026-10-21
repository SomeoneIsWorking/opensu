// vulkan_device — the runner's Vulkan instance (with the validation layer on request) and the one
// device and graphics queue it draws with, made through a Vulkan core's context negotiation when it
// has one so the core's extensions and features are on the same device; the queue's lock.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libretro_vulkan.h>

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
    /// A core's `negotiation` (may be null) supplies the application info and may create the
    /// instance itself through the runner's wrapper, which adds these extensions and layers.
    VulkanInstance(std::span<const char* const> extensions, bool validation,
                   const retro_hw_render_context_negotiation_interface_vulkan* negotiation);
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
    static VkInstance createWrapped(void* self, const VkInstanceCreateInfo* create);

    static VKAPI_ATTR VkBool32 VKAPI_CALL onMessage(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types,
        const VkDebugUtilsMessengerCallbackDataEXT* data, void* self);

    VkInstance instance_{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT messenger_{VK_NULL_HANDLE};
    std::atomic<std::size_t> errors_{0};
    /// What every instance is made with, the core's wishes added to it.
    std::vector<const char*> extensions_;
    std::vector<const char*> layers_;
};

class VulkanDevice {
  public:
    /// The best device with a graphics queue that, given a `surface`, can also present to it. With
    /// a core's `negotiation` (may be null) the core creates it on that GPU (create_device2, else
    /// create_device), the runner's extensions added; if the core declines, the runner makes it.
    VulkanDevice(const VulkanInstance& instance, VkSurfaceKHR surface,
                 const retro_hw_render_context_negotiation_interface_vulkan* negotiation);
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
    [[nodiscard]] VkQueue presentQueue() const {
        return presentQueue_;
    }

    /// The queue's lock: a core submitting on its own thread holds it too (lock_queue).
    void lockQueue() {
        queueLock_.lock();
    }
    void unlockQueue() {
        queueLock_.unlock();
    }
    /// vkQueueSubmit on the graphics queue under its lock.
    [[nodiscard]] VkResult submit(const VkSubmitInfo& submit, VkFence fence);
    /// vkQueuePresentKHR under the queue's lock.
    [[nodiscard]] VkResult present(const VkPresentInfoKHR& present);
    /// vkDeviceWaitIdle, which needs every queue, under the queue's lock.
    VkResult waitIdle();

    /// A memory type allowed by `typeBits` with all of `properties`; throws VulkanError if none.
    [[nodiscard]] std::uint32_t memoryType(std::uint32_t typeBits,
                                           VkMemoryPropertyFlags properties) const;
    /// Memory meeting `needs` with all of `properties`; throws VulkanError.
    [[nodiscard]] VkDeviceMemory allocate(VkMemoryRequirements needs,
                                          VkMemoryPropertyFlags properties) const;
    /// A colour view of the whole of single-level 2D `image`; throws VulkanError.
    [[nodiscard]] VkImageView makeView(VkImage image, VkFormat format) const;

  private:
    /// The core's device made on `gpu`; false when it declines.
    bool negotiate(const VulkanInstance& instance, VkSurfaceKHR surface);
    void createOwn();
    static VkDevice createWrapped(VkPhysicalDevice gpu, void* self,
                                  const VkDeviceCreateInfo* create);

    const retro_hw_render_context_negotiation_interface_vulkan* negotiation_;
    /// The device extensions the runner needs: the swapchain when it draws to a window.
    std::vector<const char*> extensions_;
    bool negotiated_{false};
    VkPhysicalDevice physical_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue queue_{VK_NULL_HANDLE};
    std::uint32_t queueFamily_{0};
    VkQueue presentQueue_{VK_NULL_HANDLE};
    std::mutex queueLock_;
};

} // namespace opensu::retro
