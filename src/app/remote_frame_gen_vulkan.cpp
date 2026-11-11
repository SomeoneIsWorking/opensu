#include "remote_frame_gen.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <format>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "framegen/Device.h"
#include "framegen/FramePacer.h"
#include "framegen/LosslessGenerator.h"
#include "framegen/LosslessModules.h"
#include "lucent/log.h"
#include "stream/stream_wire.hpp"

namespace opensu::app {
namespace {

/// What `pack` writes first.
constexpr std::string_view packMagic = "OSULSFG1";
/// Real frames whose images and outputs are kept: the interval shown, the one after, and the next.
constexpr std::uint64_t bufferedFrames = 3;
constexpr std::uint64_t nanosPerMicro = 1000;
constexpr std::uint32_t bytesPerPixel = 4;

void put(std::string& out, std::uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        out.push_back(static_cast<char>((value >> (8 * i)) & 0xFFU));
    }
}

/// Reads `packed` from the front; fails once anything is missing.
class Unpacker {
  public:
    explicit Unpacker(std::string_view packed) : rest_{packed} {
    }
    std::uint64_t take(int bytes) {
        if (rest_.size() < static_cast<std::size_t>(bytes)) {
            ok_ = false;
            return 0;
        }
        std::uint64_t value = 0;
        for (int i = 0; i < bytes; ++i) {
            value |= std::uint64_t{static_cast<unsigned char>(rest_[static_cast<std::size_t>(i)])}
                     << (8 * i);
        }
        rest_.remove_prefix(static_cast<std::size_t>(bytes));
        return value;
    }
    std::string_view bytes(std::size_t count) {
        if (rest_.size() < count) {
            ok_ = false;
            return {};
        }
        const std::string_view taken = rest_.substr(0, count);
        rest_.remove_prefix(count);
        return taken;
    }
    [[nodiscard]] bool whole() const noexcept {
        return ok_ && rest_.empty();
    }

  private:
    std::string_view rest_;
    bool ok_{true};
};

std::optional<framegen::LosslessModules_t> unpack(std::string_view packed) {
    Unpacker in{packed};
    if (in.bytes(packMagic.size()) != packMagic) {
        return std::nullopt;
    }
    framegen::LosslessModules_t modules;
    modules.sFileVersion = std::string{in.bytes(in.take(4))};
    modules.ulDllSize = in.take(8);
    const std::uint64_t count = in.take(4);
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto id = static_cast<std::uint32_t>(in.take(4));
        const std::string_view data = in.bytes(in.take(4));
        modules.resources[id] = std::vector<std::uint8_t>(data.begin(), data.end());
    }
    if (!in.whole()) {
        return std::nullopt;
    }
    return modules;
}

void check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error{
            std::format("{} failed: VkResult {}", what, static_cast<int>(result))};
    }
}

/// A Vulkan instance, the first GPU with what the generator needs (a discrete one before others),
/// a device on it with those features and a compute queue, its command buffers handed out and
/// submitted in order, each submission numbered by a timeline semaphore.
class GeneratorDevice final : public framegen::IQueue {
  public:
    GeneratorDevice() {
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
    ~GeneratorDevice() override {
        release();
    }
    GeneratorDevice(const GeneratorDevice&) = delete;
    GeneratorDevice& operator=(const GeneratorDevice&) = delete;
    GeneratorDevice(GeneratorDevice&&) = delete;
    GeneratorDevice& operator=(GeneratorDevice&&) = delete;

    [[nodiscard]] framegen::DeviceDesc_t desc() {
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
    [[nodiscard]] VkDevice device() const noexcept {
        return device_;
    }
    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    /// Memory of `properties` for `requirements`, bound by the caller.
    [[nodiscard]] VkDeviceMemory allocate(const VkMemoryRequirements& requirements,
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

    VkCommandBuffer Begin() override {
        VkCommandBuffer buffer = VK_NULL_HANDLE;
        if (!submitted_.empty() && submitted_.front().first <= Completed()) {
            buffer = submitted_.front().second;
            submitted_.pop_front();
            if (vkResetCommandBuffer(buffer, 0) != VK_SUCCESS) {
                return VK_NULL_HANDLE;
            }
        } else {
            const VkCommandBufferAllocateInfo info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
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

    std::uint64_t Submit(VkCommandBuffer cmd) override {
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

    std::uint64_t Completed() override {
        std::uint64_t value = 0;
        check(vkGetSemaphoreCounterValue(device_, timeline_, &value), "vkGetSemaphoreCounterValue");
        return value;
    }

    void Wait(std::uint64_t seqNo) override {
        const VkSemaphoreWaitInfo wait{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                                       .pNext = nullptr,
                                       .flags = 0,
                                       .semaphoreCount = 1,
                                       .pSemaphores = &timeline_,
                                       .pValues = &seqNo};
        check(vkWaitSemaphores(device_, &wait, UINT64_MAX), "vkWaitSemaphores");
    }

    /// Waits for everything submitted.
    [[nodiscard]] VkResult idle() noexcept {
        return vkDeviceWaitIdle(device_);
    }

  private:
    /// Picks the GPU and its compute queue; throws when none has what the shaders need.
    void choose() {
        std::uint32_t count = 0;
        check(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "vkEnumeratePhysicalDevices");
        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(instance_, &count, devices.data()),
              "vkEnumeratePhysicalDevices");
        std::stable_sort(devices.begin(), devices.end(),
                         [](VkPhysicalDevice a, VkPhysicalDevice b) {
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

    /// What `candidate` lacks of what the generator needs; empty for nothing, its features and
    /// compute queue noted.
    std::string lacks(VkPhysicalDevice candidate, const VkPhysicalDeviceProperties& properties) {
        if (properties.apiVersion < VK_API_VERSION_1_2) {
            return "Vulkan 1.2";
        }
        std::uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount,
                                             extensions.data());
        if (std::ranges::none_of(extensions, [](const VkExtensionProperties& extension) {
                return std::string_view{extension.extensionName} ==
                       VK_EXT_ROBUSTNESS_2_EXTENSION_NAME;
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
        VkPhysicalDeviceFeatures2 features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                           .pNext = &vulkan12,
                                           .features = {}};
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

    void makeDevice() {
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
        VkPhysicalDeviceFeatures2 features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                           .pNext = &vulkan12,
                                           .features = {}};
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
                                               .flags =
                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
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

    void release() noexcept {
        if (device_ != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device_);
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

/// An RGBA8 image the generator reads or writes, in GENERAL layout from its first use.
class GeneratorImage {
  public:
    GeneratorImage(const GeneratorDevice& gpu, std::uint32_t width, std::uint32_t height)
        : device_{gpu.device()}, width_{width}, height_{height} {
        const VkImageCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {.width = width, .height = height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
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
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .components = {},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}};
        check(vkCreateImageView(device_, &viewInfo, nullptr, &view_), "vkCreateImageView");
    }
    ~GeneratorImage() {
        vkDestroyImageView(device_, view_, nullptr);
        vkDestroyImage(device_, image_, nullptr);
        vkFreeMemory(device_, memory_, nullptr);
    }
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
    void toGeneral(VkCommandBuffer cmd) const {
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
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);
    }

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
    HostBuffer(const GeneratorDevice& gpu, VkDeviceSize bytes, VkBufferUsageFlags usage)
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
    ~HostBuffer() {
        vkUnmapMemory(device_, memory_);
        vkDestroyBuffer(device_, buffer_, nullptr);
        vkFreeMemory(device_, memory_, nullptr);
    }
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

framegen::PacingSettings_t pacingOf(const settings::FrameGenSettings& settings) {
    constexpr double thousand = 1000.0;
    if (settings.mode == settings::FrameGenMode::Fixed) {
        return framegen::PacingSettings_t{.eMode = framegen::PacingMode::Fixed,
                                          .flRatio = settings.ratioThousandths / thousand,
                                          .flTargetFps = 0.0,
                                          .uMaxGenerated = 3,
                                          .flToleranceFps = 0.0};
    }
    return framegen::PacingSettings_t{
        .eMode = framegen::PacingMode::Adaptive,
        .flRatio = 1.0,
        .flTargetFps = settings.targetThousandths / thousand,
        .uMaxGenerated = std::clamp(settings.maxGenerated, 1U, framegen::k_uMaxGeneratedLimit),
        .flToleranceFps = settings.toleranceThousandths / thousand};
}

class VulkanFrameGen final : public RemoteFrameGen {
  public:
    VulkanFrameGen(const settings::FrameGenSettings& settings,
                   std::shared_ptr<const framegen::LosslessModules_t> modules) {
        pacer_.Configure(pacingOf(settings));
        std::string why;
        device_ = framegen::CDevice::Create(gpu_.desc(), why);
        if (!device_) {
            throw std::runtime_error{why};
        }
        generator_ = framegen::CLosslessGenerator::Create(
            device_,
            framegen::LosslessSettings_t{
                .sDllPath = {},
                .pModules = std::move(modules),
                .eSizeMode = settings.performance ? framegen::LosslessSizeMode::Performance
                                                  : framegen::LosslessSizeMode::Balanced,
                .flFlowScale = std::clamp(static_cast<float>(settings.flowScalePercent) / 100.0F,
                                          framegen::k_flLosslessMinFlowScale, 1.0F),
                .bUiDetection = true},
            why);
        if (!generator_) {
            throw std::runtime_error{why};
        }
        lucent::info("framegen", "generating on {}, {} size mode, up to {} per real frame",
                     gpu_.name(), settings.performance ? "performance" : "balanced",
                     pacer_.GetMaxGeneratedPerFrame());
    }
    ~VulkanFrameGen() override {
        // A lost device has nothing left to wait for; the images go either way.
        static_cast<void>(gpu_.idle());
    }
    VulkanFrameGen(const VulkanFrameGen&) = delete;
    VulkanFrameGen& operator=(const VulkanFrameGen&) = delete;
    VulkanFrameGen(VulkanFrameGen&&) = delete;
    VulkanFrameGen& operator=(VulkanFrameGen&&) = delete;

    bool real(stream::GuestVideo::Frame frame, std::uint64_t refreshNs) override {
        const stream::Picture& picture = frame.picture;
        const auto width = static_cast<std::uint32_t>(picture.width);
        const auto height = static_cast<std::uint32_t>(picture.height);
        if (picture.rgba.size() != std::size_t{width} * height * bytesPerPixel) {
            lucent::warn("framegen", "frame {} is not in CPU RGBA", frame.number);
            return false;
        }
        try {
            if (width != width_ || height != height_) {
                begin(width, height);
            }
            return add(std::move(frame), refreshNs);
        } catch (const std::runtime_error& failure) {
            lucent::error("framegen", "frame generation stops: {}", failure.what());
            return false;
        }
    }

    std::optional<stream::GuestVideo::Frame> present(std::uint64_t nowUs,
                                                     std::uint64_t refreshNs) override {
        if (width_ == 0) {
            return std::nullopt;
        }
        const std::uint64_t completed = gpu_.Completed();
        const std::optional<framegen::PacedOutput_t> output = pacer_.OnVBlank(
            nowUs * nanosPerMicro, refreshNs, [this, completed](const framegen::PacedOutput_t& o) {
                const std::optional<std::uint64_t> seq = sequenceOf(o);
                return !o.oGenerated || (seq && *seq <= completed);
            });
        if (!output) {
            return std::nullopt;
        }
        ++shown_;
        const Real& interval = reals_[output->ulInterval % bufferedFrames];
        if (!output->oGenerated) {
            const Real& real = reals_[output->ulRealFrame % bufferedFrames];
            return real.frame;
        }
        ++shownGenerated_;
        const std::span<const std::uint8_t> bytes =
            readbacks_[slotOf(output->ulInterval, *output->oGenerated)]->bytes();
        return stream::GuestVideo::Frame{.number = interval.frame.number,
                                         .picture =
                                             stream::Picture{.width = static_cast<int>(width_),
                                                             .height = static_cast<int>(height_),
                                                             .rgba = {bytes.begin(), bytes.end()},
                                                             .dmabuf = std::nullopt,
                                                             .frame = std::nullopt},
                                         .capturedUs = interval.frame.capturedUs,
                                         .decodedUs = interval.frame.decodedUs};
    }

    std::string summary() const override {
        return std::format("framegen real={} shown={} generated={} dropped={} state={}", realIn_,
                           shown_, shownGenerated_, pacer_.GetDroppedOutputs(),
                           state_ ? framegen::PacingStateName(*state_) : "none");
    }

  private:
    /// A real frame kept for the intervals that show it, the submissions of its interval's
    /// generated images, and the last submission that uses its slot's image or buffers.
    struct Real {
        stream::GuestVideo::Frame frame;
        std::vector<std::uint64_t> generated;
        std::uint64_t busy{0};
    };

    [[nodiscard]] std::size_t slotOf(std::uint64_t interval, std::uint32_t generated) const {
        return static_cast<std::size_t>((interval % bufferedFrames) *
                                        pacer_.GetMaxGeneratedPerFrame()) +
               generated;
    }

    [[nodiscard]] std::optional<std::uint64_t>
    sequenceOf(const framegen::PacedOutput_t& output) const {
        const Real& interval = reals_[output.ulInterval % bufferedFrames];
        if (!output.oGenerated || *output.oGenerated >= interval.generated.size()) {
            return std::nullopt;
        }
        return interval.generated[*output.oGenerated];
    }

    /// A new session of `width` by `height`: every image and real frame before it forgotten.
    void begin(std::uint32_t width, std::uint32_t height) {
        check(gpu_.idle(), "vkDeviceWaitIdle");
        sources_.clear();
        uploads_.clear();
        outputs_.clear();
        readbacks_.clear();
        reals_ = {};
        width_ = 0;
        height_ = 0;
        const VkDeviceSize bytes = VkDeviceSize{width} * height * bytesPerPixel;
        for (std::uint64_t i = 0; i < bufferedFrames; ++i) {
            sources_.push_back(std::make_unique<GeneratorImage>(gpu_, width, height));
            uploads_.push_back(
                std::make_unique<HostBuffer>(gpu_, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
        }
        const std::size_t outputs = bufferedFrames * pacer_.GetMaxGeneratedPerFrame();
        for (std::size_t i = 0; i < outputs; ++i) {
            outputs_.push_back(std::make_unique<GeneratorImage>(gpu_, width, height));
            readbacks_.push_back(
                std::make_unique<HostBuffer>(gpu_, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
        }
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            throw std::runtime_error{"no command buffer for the images"};
        }
        for (const auto& image : sources_) {
            image->toGeneral(cmd);
        }
        for (const auto& image : outputs_) {
            image->toGeneral(cmd);
        }
        gpu_.Wait(gpu_.Submit(cmd));
        pacer_.Reset();
        frames_ = 0;
        lastAdded_.reset();
        width_ = width;
        height_ = height;
        lucent::info("framegen", "session at {}x{}", width, height);
    }

    bool add(stream::GuestVideo::Frame frame, std::uint64_t refreshNs) {
        const std::uint64_t index = frames_;
        if (index >= 1) {
            // The images the upload and this interval overwrite.
            const std::uint64_t oldest =
                index > bufferedFrames - 1 ? index - (bufferedFrames - 1) : 0;
            pacer_.DropOlderThan(oldest, oldest);
        }
        Real& slot = reals_[index % bufferedFrames];
        const GeneratorImage& source = *sources_[index % bufferedFrames];
        const HostBuffer& upload = *uploads_[index % bufferedFrames];
        // Whatever still uses the slot's image or buffers finishes first.
        gpu_.Wait(slot.busy);
        const std::span<std::uint8_t> staged = upload.bytes();
        std::memcpy(staged.data(), frame.picture.rgba.data(), staged.size());
        VkCommandBuffer cmd = gpu_.Begin();
        if (cmd == VK_NULL_HANDLE) {
            lucent::error("framegen", "no command buffer for frame {}", frame.number);
            return false;
        }
        memoryBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkBufferImageCopy region = wholeImage(width_, height_);
        vkCmdCopyBufferToImage(cmd, upload.buffer(), source.image(), VK_IMAGE_LAYOUT_GENERAL, 1,
                               &region);
        slot.busy = gpu_.Submit(cmd);
        slot.generated.clear();

        const std::uint64_t readyNs = frame.capturedUs * nanosPerMicro;
        // When it reached frame generation, which places it on the output timeline.
        const std::uint64_t arrivalNs = stream::wire::nowMicros() * nanosPerMicro;
        slot.frame = std::move(frame);
        const framegen::IntervalPlan_t plan =
            pacer_.OnRealFrame(index, readyNs, arrivalNs, refreshNs);
        if (plan.eState == framegen::PacingState::Generating) {
            const bool follows = lastAdded_ && *lastAdded_ + 1 == index;
            if (!follows && !generator_->BeginSession(width_, height_)) {
                lucent::error("framegen", "the generator does not restart at frame {}",
                              slot.frame.number);
                return false;
            }
            const std::optional<framegen::Submission_t> added =
                generator_->AddRealFrame(source.view());
            if (!added || !added->bComplete) {
                lucent::error("framegen", "frame {} did not go into the generator",
                              slot.frame.number);
                return false;
            }
            slot.busy = added->ulSeqNo;
            lastAdded_ = index;
        }
        if (!state_ || *state_ != plan.eState) {
            lucent::info("framegen", "pacing {} at frame {}: base {:.2f} fps, target {:.2f} fps",
                         framegen::PacingStateName(plan.eState), slot.frame.number, plan.flBaseFps,
                         plan.flTargetFps);
            state_ = plan.eState;
        }
        for (std::size_t i = 0; i < plan.flTimestamps.size(); ++i) {
            const std::size_t at = slotOf(index, static_cast<std::uint32_t>(i));
            const GeneratorImage& output = *outputs_[at];
            const HostBuffer& readback = *readbacks_[at];
            const framegen::OutputHooks_t hooks{
                .before = {}, .after = [this, &output, &readback](VkCommandBuffer recording) {
                    memoryBarrier(recording, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_ACCESS_TRANSFER_READ_BIT);
                    const VkBufferImageCopy copy = wholeImage(width_, height_);
                    vkCmdCopyImageToBuffer(recording, output.image(), VK_IMAGE_LAYOUT_GENERAL,
                                           readback.buffer(), 1, &copy);
                    memoryBarrier(recording, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  VK_ACCESS_HOST_READ_BIT);
                }};
            const std::optional<framegen::Submission_t> made =
                generator_->Generate(plan.flTimestamps[i], output.view(), hooks);
            if (!made || !made->bComplete) {
                lucent::error("framegen", "no image generated before frame {}", slot.frame.number);
                return false;
            }
            slot.generated.push_back(made->ulSeqNo);
            slot.busy = made->ulSeqNo;
        }
        ++frames_;
        ++realIn_;
        return true;
    }

    GeneratorDevice gpu_;
    std::shared_ptr<framegen::CDevice> device_;
    std::unique_ptr<framegen::CLosslessGenerator> generator_;
    framegen::CFramePacer pacer_;
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::vector<std::unique_ptr<GeneratorImage>> sources_;
    std::vector<std::unique_ptr<HostBuffer>> uploads_;
    std::vector<std::unique_ptr<GeneratorImage>> outputs_;
    std::vector<std::unique_ptr<HostBuffer>> readbacks_;
    std::array<Real, bufferedFrames> reals_{};
    /// Real frames since the session began, the last of them the generator took, and its state.
    std::uint64_t frames_{0};
    std::optional<std::uint64_t> lastAdded_;
    std::optional<framegen::PacingState> state_;
    std::uint64_t realIn_{0};
    std::uint64_t shown_{0};
    std::uint64_t shownGenerated_{0};
};

} // namespace

bool RemoteFrameGen::built() noexcept {
    return true;
}

std::optional<std::string> RemoteFrameGen::pack(const std::filesystem::path& dll,
                                                std::string& why) {
    const std::optional<framegen::LosslessModules_t> modules =
        framegen::ReadLosslessModules(dll.string(), why);
    if (!modules) {
        return std::nullopt;
    }
    std::string out{packMagic};
    put(out, modules->sFileVersion.size(), 4);
    out += modules->sFileVersion;
    put(out, modules->ulDllSize, 8);
    put(out, modules->resources.size(), 4);
    for (const auto& [id, data] : modules->resources) {
        put(out, id, 4);
        put(out, data.size(), 4);
        out.append(data.begin(), data.end());
    }
    return out;
}

std::unique_ptr<RemoteFrameGen> RemoteFrameGen::open(const settings::FrameGenSettings& settings,
                                                     std::string_view packed, std::string& why) {
    std::optional<framegen::LosslessModules_t> modules = unpack(packed);
    if (!modules) {
        why = "the host's shaders did not arrive whole";
        return nullptr;
    }
    try {
        return std::make_unique<VulkanFrameGen>(
            settings, std::make_shared<const framegen::LosslessModules_t>(std::move(*modules)));
    } catch (const std::runtime_error& failure) {
        why = failure.what();
        return nullptr;
    }
}

} // namespace opensu::app
