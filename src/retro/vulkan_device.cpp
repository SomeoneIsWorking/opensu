#include "vulkan_device.hpp"

#include <cstring>
#include <optional>
#include <vector>

#include "lucent/log.h"

namespace opensu::retro {
namespace {

constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";

bool layerInstalled(const char* name) {
    std::uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const VkLayerProperties& layer : layers) {
        if (std::strcmp(layer.layerName, name) == 0) {
            return true;
        }
    }
    return false;
}

/// Higher for the devices a player would rather draw on.
int preference(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 4;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return 3;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return 1;
    default:
        return 0;
    }
}

/// A queue family that draws and, given a surface, presents to it.
std::optional<std::uint32_t> drawingFamily(VkPhysicalDevice device, VkSurfaceKHR surface) {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    for (std::uint32_t family = 0; family < count; ++family) {
        if ((families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
            continue;
        }
        VkBool32 presents = VK_TRUE;
        if (surface != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceSurfaceSupportKHR(device, family, surface, &presents);
        }
        if (presents == VK_TRUE) {
            return family;
        }
    }
    return std::nullopt;
}

} // namespace

void check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw VulkanError{std::string{what} + " failed: VkResult " +
                          std::to_string(static_cast<int>(result))};
    }
}

VulkanInstance::VulkanInstance(std::span<const char* const> extensions, bool validation) {
    std::vector<const char*> enabled(extensions.begin(), extensions.end());
    std::vector<const char*> layers;
    if (validation) {
        if (layerInstalled(validationLayer)) {
            layers.push_back(validationLayer);
            enabled.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        } else {
            lucent::warn("vulkan", "{} is not installed; running without validation",
                         validationLayer);
        }
    }
    VkApplicationInfo application{};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "opensu-retro";
    application.pEngineName = "opensu";
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create.pApplicationInfo = &application;
    create.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
    create.ppEnabledLayerNames = layers.data();
    create.enabledExtensionCount = static_cast<std::uint32_t>(enabled.size());
    create.ppEnabledExtensionNames = enabled.data();
    check(vkCreateInstance(&create, nullptr, &instance_), "vkCreateInstance");
    if (layers.empty()) {
        return;
    }
    VkDebugUtilsMessengerCreateInfoEXT messenger{};
    messenger.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messenger.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messenger.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messenger.pfnUserCallback = onMessage;
    messenger.pUserData = this;
    const auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    if (createMessenger == nullptr) {
        vkDestroyInstance(instance_, nullptr);
        throw VulkanError{"the validation layer has no debug messenger"};
    }
    const VkResult made = createMessenger(instance_, &messenger, nullptr, &messenger_);
    if (made != VK_SUCCESS) {
        vkDestroyInstance(instance_, nullptr);
        check(made, "vkCreateDebugUtilsMessengerEXT");
    }
    lucent::info("vulkan", "validation on");
}

VulkanInstance::~VulkanInstance() {
    if (messenger_ != VK_NULL_HANDLE) {
        const auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        destroyMessenger(instance_, messenger_, nullptr);
    }
    vkDestroyInstance(instance_, nullptr);
}

bool isValidationError(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                       VkDebugUtilsMessageTypeFlagsEXT types) {
    return (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0 &&
           (types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanInstance::onMessage(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* self) {
    const char* message = data != nullptr && data->pMessage != nullptr ? data->pMessage : "";
    if (isValidationError(severity, types)) {
        ++static_cast<VulkanInstance*>(self)->errors_;
        lucent::error("vulkan", "{}", message);
    } else {
        lucent::warn("vulkan", "{}", message);
    }
    return VK_FALSE;
}

VulkanDevice::VulkanDevice(const VulkanInstance& instance, VkSurfaceKHR surface) {
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance.handle(), &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance.handle(), &count, devices.data());
    int best = -1;
    for (VkPhysicalDevice device : devices) {
        const std::optional<std::uint32_t> family = drawingFamily(device, surface);
        if (!family) {
            continue;
        }
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        if (preference(properties.deviceType) > best) {
            best = preference(properties.deviceType);
            physical_ = device;
            queueFamily_ = *family;
        }
    }
    if (physical_ == VK_NULL_HANDLE) {
        throw VulkanError{"no Vulkan device can draw to this window"};
    }
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_, &properties);
    lucent::info("vulkan", "drawing on {}", properties.deviceName);

    const float priority = 1.0F;
    VkDeviceQueueCreateInfo queue{};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = queueFamily_;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    const char* swapchain = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create.queueCreateInfoCount = 1;
    create.pQueueCreateInfos = &queue;
    if (surface != VK_NULL_HANDLE) {
        create.enabledExtensionCount = 1;
        create.ppEnabledExtensionNames = &swapchain;
    }
    check(vkCreateDevice(physical_, &create, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
}

VulkanDevice::~VulkanDevice() {
    vkDestroyDevice(device_, nullptr);
}

std::uint32_t VulkanDevice::memoryType(std::uint32_t typeBits,
                                       VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memory{};
    vkGetPhysicalDeviceMemoryProperties(physical_, &memory);
    for (std::uint32_t type = 0; type < memory.memoryTypeCount; ++type) {
        if ((typeBits & (1U << type)) != 0 &&
            (memory.memoryTypes[type].propertyFlags & properties) == properties) {
            return type;
        }
    }
    throw VulkanError{"no suitable Vulkan memory type"};
}

VkDeviceMemory VulkanDevice::allocate(VkMemoryRequirements needs,
                                      VkMemoryPropertyFlags properties) const {
    VkMemoryAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = needs.size;
    allocate.memoryTypeIndex = memoryType(needs.memoryTypeBits, properties);
    VkDeviceMemory memory = VK_NULL_HANDLE;
    check(vkAllocateMemory(device_, &allocate, nullptr, &memory), "vkAllocateMemory");
    return memory;
}

VkImageView VulkanDevice::makeView(VkImage image, VkFormat format) const {
    VkImageViewCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create.image = image;
    create.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create.format = format;
    create.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    check(vkCreateImageView(device_, &create, nullptr, &view), "vkCreateImageView");
    return view;
}

VkImageCreateInfo imageInfo(VkFormat format, VkExtent2D extent, VkImageUsageFlags usage) {
    return VkImageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {extent.width, extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
}

} // namespace opensu::retro
