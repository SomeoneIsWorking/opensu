#include "vulkan_device.hpp"

#include <algorithm>
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

/// `wanted` with each of `required` it lacks appended.
std::vector<const char*> withAll(std::span<const char* const> wanted,
                                 std::span<const char* const> required) {
    std::vector<const char*> all(wanted.begin(), wanted.end());
    for (const char* name : required) {
        if (std::ranges::none_of(all, [name](const char* had) {
                return std::strcmp(had, name) == 0;
            })) {
            all.push_back(name);
        }
    }
    return all;
}

} // namespace

void check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw VulkanError{std::string{what} + " failed: VkResult " +
                          std::to_string(static_cast<int>(result))};
    }
}

VulkanInstance::VulkanInstance(
    std::span<const char* const> extensions, bool validation,
    const retro_hw_render_context_negotiation_interface_vulkan* negotiation)
    : extensions_(extensions.begin(), extensions.end()) {
    if (validation) {
        if (layerInstalled(validationLayer)) {
            layers_.push_back(validationLayer);
            extensions_.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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
    if (negotiation != nullptr && negotiation->get_application_info != nullptr) {
        if (const VkApplicationInfo* wanted = negotiation->get_application_info()) {
            application = *wanted;
            application.apiVersion = std::max(application.apiVersion, VK_API_VERSION_1_1);
        }
    }
    if (negotiation != nullptr && negotiation->interface_version >= 2 &&
        negotiation->create_instance != nullptr) {
        instance_ =
            negotiation->create_instance(vkGetInstanceProcAddr, &application, createWrapped, this);
        if (instance_ == VK_NULL_HANDLE) {
            throw VulkanError{"the core could not create its Vulkan instance"};
        }
    } else {
        VkInstanceCreateInfo create{};
        create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create.pApplicationInfo = &application;
        create.enabledLayerCount = static_cast<std::uint32_t>(layers_.size());
        create.ppEnabledLayerNames = layers_.data();
        create.enabledExtensionCount = static_cast<std::uint32_t>(extensions_.size());
        create.ppEnabledExtensionNames = extensions_.data();
        check(vkCreateInstance(&create, nullptr, &instance_), "vkCreateInstance");
    }
    if (layers_.empty()) {
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

VkInstance VulkanInstance::createWrapped(void* self, const VkInstanceCreateInfo* create) {
    const auto& runner = *static_cast<VulkanInstance*>(self);
    std::vector<const char*> extensions = withAll(
        {create->ppEnabledExtensionNames, create->enabledExtensionCount}, runner.extensions_);
    std::vector<const char*> layers =
        withAll({create->ppEnabledLayerNames, create->enabledLayerCount}, runner.layers_);
    VkInstanceCreateInfo merged = *create;
    merged.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    merged.ppEnabledExtensionNames = extensions.data();
    merged.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
    merged.ppEnabledLayerNames = layers.data();
    VkInstance instance = VK_NULL_HANDLE;
    const VkResult made = vkCreateInstance(&merged, nullptr, &instance);
    if (made != VK_SUCCESS) {
        lucent::error("vulkan", "the core's vkCreateInstance failed: VkResult {}",
                      static_cast<int>(made));
        return VK_NULL_HANDLE;
    }
    return instance;
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

VulkanDevice::VulkanDevice(const VulkanInstance& instance, VkSurfaceKHR surface,
                           const retro_hw_render_context_negotiation_interface_vulkan* negotiation)
    : negotiation_{negotiation} {
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
    if (surface != VK_NULL_HANDLE) {
        extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    if (negotiation_ == nullptr || !negotiate(instance, surface)) {
        createOwn();
    }
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_, &properties);
    lucent::info("vulkan", "drawing on {}{}", properties.deviceName,
                 negotiated_ ? ", the device made by the core" : "");
}

bool VulkanDevice::negotiate(const VulkanInstance& instance, VkSurfaceKHR surface) {
    retro_vulkan_context context{};
    bool made = false;
    if (negotiation_->interface_version >= 2 && negotiation_->create_device2 != nullptr) {
        made = negotiation_->create_device2(&context, instance.handle(), physical_, surface,
                                            vkGetInstanceProcAddr, createWrapped, this);
        if (!made) {
            lucent::warn("vulkan", "the core cannot use this GPU; letting it choose");
            context = {};
            made =
                negotiation_->create_device2(&context, instance.handle(), VK_NULL_HANDLE, surface,
                                             vkGetInstanceProcAddr, createWrapped, this);
        }
    } else if (negotiation_->create_device != nullptr) {
        // The runner needs no features; cores read the struct, so it is never null.
        const VkPhysicalDeviceFeatures features{};
        made = negotiation_->create_device(
            &context, instance.handle(), physical_, surface, vkGetInstanceProcAddr,
            extensions_.data(), static_cast<unsigned>(extensions_.size()), nullptr, 0, &features);
    } else {
        return false;
    }
    if (!made) {
        lucent::warn("vulkan", "the core made no device; the runner makes its own");
        return false;
    }
    negotiated_ = true;
    physical_ = context.gpu;
    device_ = context.device;
    queue_ = context.queue;
    queueFamily_ = context.queue_family_index;
    presentQueue_ =
        context.presentation_queue != VK_NULL_HANDLE ? context.presentation_queue : context.queue;
    return true;
}

VkDevice VulkanDevice::createWrapped(VkPhysicalDevice gpu, void* self,
                                     const VkDeviceCreateInfo* create) {
    const auto& runner = *static_cast<VulkanDevice*>(self);
    std::vector<const char*> extensions = withAll(
        {create->ppEnabledExtensionNames, create->enabledExtensionCount}, runner.extensions_);
    VkDeviceCreateInfo merged = *create;
    merged.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    merged.ppEnabledExtensionNames = extensions.data();
    VkDevice device = VK_NULL_HANDLE;
    const VkResult made = vkCreateDevice(gpu, &merged, nullptr, &device);
    if (made != VK_SUCCESS) {
        lucent::error("vulkan", "the core's vkCreateDevice failed: VkResult {}",
                      static_cast<int>(made));
        return VK_NULL_HANDLE;
    }
    return device;
}

void VulkanDevice::createOwn() {
    const float priority = 1.0F;
    VkDeviceQueueCreateInfo queue{};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = queueFamily_;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create.queueCreateInfoCount = 1;
    create.pQueueCreateInfos = &queue;
    create.enabledExtensionCount = static_cast<std::uint32_t>(extensions_.size());
    create.ppEnabledExtensionNames = extensions_.data();
    check(vkCreateDevice(physical_, &create, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
    presentQueue_ = queue_;
}

VulkanDevice::~VulkanDevice() {
    vkDestroyDevice(device_, nullptr);
    if (negotiated_ && negotiation_->destroy_device != nullptr) {
        negotiation_->destroy_device();
    }
}

VkResult VulkanDevice::submit(const VkSubmitInfo& submit, VkFence fence) {
    const std::scoped_lock held{queueLock_};
    return vkQueueSubmit(queue_, 1, &submit, fence);
}

VkResult VulkanDevice::present(const VkPresentInfoKHR& present) {
    const std::scoped_lock held{queueLock_};
    return vkQueuePresentKHR(presentQueue_, &present);
}

VkResult VulkanDevice::waitIdle() {
    const std::scoped_lock held{queueLock_};
    return vkDeviceWaitIdle(device_);
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

VkShaderModule makeShader(VkDevice device, std::span<const std::uint32_t> code) {
    VkShaderModuleCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create.codeSize = code.size_bytes();
    create.pCode = code.data();
    VkShaderModule module = VK_NULL_HANDLE;
    check(vkCreateShaderModule(device, &create, nullptr, &module), "vkCreateShaderModule");
    return module;
}

void imageBarrier(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to,
                  VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess,
                  VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(commands, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

} // namespace opensu::retro
