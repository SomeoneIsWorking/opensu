#include "frame_gen/generator_gpu.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "lucent/log.h"

namespace opensu::frame_gen {

void check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error{
            std::format("{} failed: VkResult {}", what, static_cast<int>(result))};
    }
}

GeneratorDevice::GeneratorDevice() {
    const VkApplicationInfo application{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                        .pNext = nullptr,
                                        .pApplicationName = "openSU frame generation",
                                        .applicationVersion = 1,
                                        .pEngineName = "openSU",
                                        .engineVersion = 1,
                                        .apiVersion = VK_API_VERSION_1_2};
    const VkInstanceCreateInfo instanceInfo{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                            .pNext = nullptr,
                                            .flags = 0,
                                            .pApplicationInfo = &application,
                                            .enabledLayerCount = 0,
                                            .ppEnabledLayerNames = nullptr,
                                            .enabledExtensionCount = 0,
                                            .ppEnabledExtensionNames = nullptr};
    check(vkCreateInstance(&instanceInfo, nullptr, &instance_), "vkCreateInstance");
    try {
        choose();
        makeDevice();
    } catch (...) {
        release();
        throw;
    }
}

GeneratorDevice::~GeneratorDevice() {
    release();
}

framegen::DeviceDesc_t GeneratorDevice::desc() {
    return framegen::DeviceDesc_t{.pfnGetInstanceProcAddr = vkGetInstanceProcAddr,
                                  .instance = instance_,
                                  .physicalDevice = physical_,
                                  .device = device_,
                                  .bFloat16 = float16_,
                                  .bStorageWriteWithoutFormat = true,
                                  .bStorageExtendedFormats = true,
                                  .pQueue = this,
                                  .log = [](framegen::LogLevel level, const std::string& line) {
                                      switch (level) {
                                      case framegen::LogLevel::Error:
                                          lucent::error("framegen", "{}", line);
                                          break;
                                      case framegen::LogLevel::Info:
                                          lucent::info("framegen", "{}", line);
                                          break;
                                      case framegen::LogLevel::Debug:
                                          lucent::debug("framegen", "{}", line);
                                          break;
                                      }
                                  }};
}

VkDeviceMemory GeneratorDevice::allocate(const VkMemoryRequirements& requirements,
                                         VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memory{};
    vkGetPhysicalDeviceMemoryProperties(physical_, &memory);
    for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        if ((requirements.memoryTypeBits & (1U << i)) != 0 &&
            (memory.memoryTypes[i].propertyFlags & properties) == properties) {
            const VkMemoryAllocateInfo info{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                            .pNext = nullptr,
                                            .allocationSize = requirements.size,
                                            .memoryTypeIndex = i};
            VkDeviceMemory allocated = VK_NULL_HANDLE;
            check(vkAllocateMemory(device_, &info, nullptr, &allocated), "vkAllocateMemory");
            return allocated;
        }
    }
    throw std::runtime_error{"the GPU has no memory of the kind needed"};
}

VkCommandBuffer GeneratorDevice::Begin() {
    VkCommandBuffer buffer = VK_NULL_HANDLE;
    if (!submitted_.empty() && submitted_.front().first <= Completed()) {
        buffer = submitted_.front().second;
        submitted_.pop_front();
        if (vkResetCommandBuffer(buffer, 0) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
    } else {
        const VkCommandBufferAllocateInfo info{.sType =
                                                   VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                               .pNext = nullptr,
                                               .commandPool = pool_,
                                               .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                               .commandBufferCount = 1};
        if (vkAllocateCommandBuffers(device_, &info, &buffer) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
    }
    const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                         .pNext = nullptr,
                                         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                         .pInheritanceInfo = nullptr};
    if (vkBeginCommandBuffer(buffer, &begin) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, pool_, 1, &buffer);
        return VK_NULL_HANDLE;
    }
    return buffer;
}

std::uint64_t GeneratorDevice::Submit(VkCommandBuffer cmd) {
    check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
    const std::uint64_t number = submittedSeq_ + 1;
    const VkTimelineSemaphoreSubmitInfo timeline{
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreValueCount = 0,
        .pWaitSemaphoreValues = nullptr,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &number};
    const VkSubmitInfo submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .pNext = &timeline,
                              .waitSemaphoreCount = 0,
                              .pWaitSemaphores = nullptr,
                              .pWaitDstStageMask = nullptr,
                              .commandBufferCount = 1,
                              .pCommandBuffers = &cmd,
                              .signalSemaphoreCount = 1,
                              .pSignalSemaphores = &timeline_};
    check(vkQueueSubmit(queue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit");
    submittedSeq_ = number;
    submitted_.emplace_back(number, cmd);
    return number;
}

std::uint64_t GeneratorDevice::Completed() {
    std::uint64_t value = 0;
    check(vkGetSemaphoreCounterValue(device_, timeline_, &value), "vkGetSemaphoreCounterValue");
    return value;
}

void GeneratorDevice::Wait(std::uint64_t seqNo) {
    const VkSemaphoreWaitInfo wait{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                                   .pNext = nullptr,
                                   .flags = 0,
                                   .semaphoreCount = 1,
                                   .pSemaphores = &timeline_,
                                   .pValues = &seqNo};
    check(vkWaitSemaphores(device_, &wait, UINT64_MAX), "vkWaitSemaphores");
    if (spans_) {
        spans_->drain(seqNo);
    }
}

bool GeneratorDevice::reached(std::uint64_t seqNo, std::uint64_t timeoutNs) {
    const VkSemaphoreWaitInfo wait{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                                   .pNext = nullptr,
                                   .flags = 0,
                                   .semaphoreCount = 1,
                                   .pSemaphores = &timeline_,
                                   .pValues = &seqNo};
    const VkResult result = vkWaitSemaphores(device_, &wait, timeoutNs);
    if (result == VK_TIMEOUT) {
        return false;
    }
    check(result, "vkWaitSemaphores");
    return true;
}

void GeneratorDevice::measure(Measure what) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_, &properties);
    std::uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_, &familyCount, families.data());
    spans_ = std::make_unique<GpuSpans>(device_, properties.limits.timestampPeriod,
                                        families.at(family_).timestampValidBits, what);
}

std::optional<std::uint32_t> GeneratorDevice::BeginTiming(VkCommandBuffer cmd) {
    return spans_ ? spans_->begin(cmd) : std::nullopt;
}

void GeneratorDevice::EndTiming(VkCommandBuffer cmd, std::uint32_t slot, const std::string& key) {
    if (spans_) {
        spans_->end(cmd, slot, key);
    }
}

void GeneratorDevice::TimingSubmitted(std::uint64_t seqNo) {
    if (spans_) {
        spans_->submitted(seqNo);
    }
}

VkResult GeneratorDevice::idle() noexcept {
    return vkDeviceWaitIdle(device_);
}

void GeneratorDevice::choose() {
    std::uint32_t count = 0;
    check(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "vkEnumeratePhysicalDevices");
    std::vector<VkPhysicalDevice> devices(count);
    check(vkEnumeratePhysicalDevices(instance_, &count, devices.data()),
          "vkEnumeratePhysicalDevices");
    std::stable_sort(devices.begin(), devices.end(), [](VkPhysicalDevice a, VkPhysicalDevice b) {
        VkPhysicalDeviceProperties pa{};
        VkPhysicalDeviceProperties pb{};
        vkGetPhysicalDeviceProperties(a, &pa);
        vkGetPhysicalDeviceProperties(b, &pb);
        return pa.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
               pb.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    });
    std::string missing = "no GPU";
    for (VkPhysicalDevice candidate : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate, &properties);
        const std::string why = lacks(candidate, properties);
        if (why.empty()) {
            physical_ = candidate;
            name_ = properties.deviceName;
            return;
        }
        missing = std::format("{} lacks {}", properties.deviceName, why);
    }
    throw std::runtime_error{missing};
}

/// What `candidate` lacks of what the generators need; empty for nothing, its features and
/// compute queue noted.
std::string GeneratorDevice::lacks(VkPhysicalDevice candidate,
                                   const VkPhysicalDeviceProperties& properties) {
    if (properties.apiVersion < VK_API_VERSION_1_2) {
        return "Vulkan 1.2";
    }
    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, extensions.data());
    if (std::ranges::none_of(extensions, [](const VkExtensionProperties& extension) {
            return std::string_view{extension.extensionName} == VK_EXT_ROBUSTNESS_2_EXTENSION_NAME;
        })) {
        return VK_EXT_ROBUSTNESS_2_EXTENSION_NAME;
    }
    VkPhysicalDeviceRobustness2FeaturesEXT robustness{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = nullptr,
        .robustBufferAccess2 = VK_FALSE,
        .robustImageAccess2 = VK_FALSE,
        .nullDescriptor = VK_FALSE};
    VkPhysicalDeviceVulkan12Features vulkan12{};
    vulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12.pNext = &robustness;
    VkPhysicalDeviceFeatures2 features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &vulkan12, .features = {}};
    vkGetPhysicalDeviceFeatures2(candidate, &features);
    if (robustness.nullDescriptor == VK_FALSE) {
        return "null descriptors";
    }
    if (vulkan12.timelineSemaphore == VK_FALSE) {
        return "timeline semaphores";
    }
    if (features.features.shaderStorageImageWriteWithoutFormat == VK_FALSE ||
        features.features.shaderStorageImageExtendedFormats == VK_FALSE) {
        return "storage images of any format";
    }
    std::uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());
    const auto compute = std::ranges::find_if(families, [](const VkQueueFamilyProperties& f) {
        return (f.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
    });
    if (compute == families.end()) {
        return "a compute queue";
    }
    family_ = static_cast<std::uint32_t>(compute - families.begin());
    float16_ = vulkan12.shaderFloat16 == VK_TRUE && features.features.shaderInt16 == VK_TRUE;
    scalarLayout_ = vulkan12.scalarBlockLayout == VK_TRUE;
    return {};
}

void GeneratorDevice::makeDevice() {
    const float priority = 1.0F;
    const VkDeviceQueueCreateInfo queueInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                            .pNext = nullptr,
                                            .flags = 0,
                                            .queueFamilyIndex = family_,
                                            .queueCount = 1,
                                            .pQueuePriorities = &priority};
    VkPhysicalDeviceRobustness2FeaturesEXT robustness{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = nullptr,
        .robustBufferAccess2 = VK_FALSE,
        .robustImageAccess2 = VK_FALSE,
        .nullDescriptor = VK_TRUE};
    VkPhysicalDeviceVulkan12Features vulkan12{};
    vulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12.pNext = &robustness;
    vulkan12.shaderFloat16 = float16_ ? VK_TRUE : VK_FALSE;
    vulkan12.scalarBlockLayout = scalarLayout_ ? VK_TRUE : VK_FALSE;
    vulkan12.timelineSemaphore = VK_TRUE;
    VkPhysicalDeviceFeatures2 features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &vulkan12, .features = {}};
    features.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
    features.features.shaderStorageImageExtendedFormats = VK_TRUE;
    features.features.shaderInt16 = float16_ ? VK_TRUE : VK_FALSE;
    const std::array<const char*, 1> extensions{VK_EXT_ROBUSTNESS_2_EXTENSION_NAME};
    const VkDeviceCreateInfo deviceInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                        .pNext = &features,
                                        .flags = 0,
                                        .queueCreateInfoCount = 1,
                                        .pQueueCreateInfos = &queueInfo,
                                        .enabledLayerCount = 0,
                                        .ppEnabledLayerNames = nullptr,
                                        .enabledExtensionCount =
                                            static_cast<std::uint32_t>(extensions.size()),
                                        .ppEnabledExtensionNames = extensions.data(),
                                        .pEnabledFeatures = nullptr};
    check(vkCreateDevice(physical_, &deviceInfo, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, family_, 0, &queue_);
    const VkCommandPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                           .pNext = nullptr,
                                           .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                           .queueFamilyIndex = family_};
    check(vkCreateCommandPool(device_, &poolInfo, nullptr, &pool_), "vkCreateCommandPool");
    const VkSemaphoreTypeCreateInfo type{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                         .pNext = nullptr,
                                         .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                                         .initialValue = 0};
    const VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &type, .flags = 0};
    check(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &timeline_), "vkCreateSemaphore");
}

void GeneratorDevice::release() noexcept {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        spans_.reset();
        if (timeline_ != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, timeline_, nullptr);
        }
        if (pool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, pool_, nullptr);
        }
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

namespace {

std::uint64_t texelBytes(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_UINT:
        return 4;
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return 8;
    default:
        throw std::runtime_error{
            std::format("no texel size for image format {}", static_cast<int>(format))};
    }
}

} // namespace

GeneratorImage::GeneratorImage(const GeneratorDevice& gpu, std::uint32_t width,
                               std::uint32_t height, VkFormat format)
    : device_{gpu.device()}, width_{width}, height_{height},
      bytes_{std::uint64_t{width} * height * texelBytes(format)} {
    const VkImageCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                 .pNext = nullptr,
                                 .flags = 0,
                                 .imageType = VK_IMAGE_TYPE_2D,
                                 .format = format,
                                 .extent = {.width = width, .height = height, .depth = 1},
                                 .mipLevels = 1,
                                 .arrayLayers = 1,
                                 .samples = VK_SAMPLE_COUNT_1_BIT,
                                 .tiling = VK_IMAGE_TILING_OPTIMAL,
                                 .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                 .queueFamilyIndexCount = 0,
                                 .pQueueFamilyIndices = nullptr,
                                 .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    check(vkCreateImage(device_, &info, nullptr, &image_), "vkCreateImage");
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, image_, &requirements);
    memory_ = gpu.allocate(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkBindImageMemory(device_, image_, memory_, 0), "vkBindImageMemory");
    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {},
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    check(vkCreateImageView(device_, &viewInfo, nullptr, &view_), "vkCreateImageView");
}

GeneratorImage::~GeneratorImage() {
    vkDestroyImageView(device_, view_, nullptr);
    vkDestroyImage(device_, image_, nullptr);
    vkFreeMemory(device_, memory_, nullptr);
}

void GeneratorImage::toGeneral(VkCommandBuffer cmd) const {
    const VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                         VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image_,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

HostBuffer::HostBuffer(const GeneratorDevice& gpu, VkDeviceSize bytes, VkBufferUsageFlags usage)
    : device_{gpu.device()}, bytes_{bytes} {
    const VkBufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                  .pNext = nullptr,
                                  .flags = 0,
                                  .size = bytes,
                                  .usage = usage,
                                  .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                  .queueFamilyIndexCount = 0,
                                  .pQueueFamilyIndices = nullptr};
    check(vkCreateBuffer(device_, &info, nullptr, &buffer_), "vkCreateBuffer");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer_, &requirements);
    const VkMemoryPropertyFlags shared =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    // Reads from cached memory are many times quicker; writes need no cache.
    try {
        memory_ = gpu.allocate(requirements, (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0
                                                 ? shared | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
                                                 : shared);
    } catch (const std::runtime_error&) {
        memory_ = gpu.allocate(requirements, shared);
    }
    check(vkBindBufferMemory(device_, buffer_, memory_, 0), "vkBindBufferMemory");
    check(vkMapMemory(device_, memory_, 0, bytes, 0, &mapped_), "vkMapMemory");
}

HostBuffer::~HostBuffer() {
    vkUnmapMemory(device_, memory_);
    vkDestroyBuffer(device_, buffer_, nullptr);
    vkFreeMemory(device_, memory_, nullptr);
}

VkBufferImageCopy wholeImage(std::uint32_t width, std::uint32_t height) {
    return VkBufferImageCopy{.bufferOffset = 0,
                             .bufferRowLength = 0,
                             .bufferImageHeight = 0,
                             .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                  .mipLevel = 0,
                                                  .baseArrayLayer = 0,
                                                  .layerCount = 1},
                             .imageOffset = {.x = 0, .y = 0, .z = 0},
                             .imageExtent = {.width = width, .height = height, .depth = 1}};
}

void memoryBarrier(VkCommandBuffer cmd, VkPipelineStageFlags from, VkAccessFlags written,
                   VkPipelineStageFlags to, VkAccessFlags read) {
    const VkMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                  .pNext = nullptr,
                                  .srcAccessMask = written,
                                  .dstAccessMask = read};
    vkCmdPipelineBarrier(cmd, from, to, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

} // namespace opensu::frame_gen
