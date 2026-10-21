#include "hardware_render.hpp"

#include <utility>

#include "lucent/log.h"

namespace opensu::retro {
namespace {

/// The context a core asked for, as a player would name it.
std::string contextName(const retro_hw_render_callback& callback) {
    const std::string version =
        std::to_string(callback.version_major) + "." + std::to_string(callback.version_minor);
    switch (callback.context_type) {
    case RETRO_HW_CONTEXT_OPENGL:
        return "OpenGL";
    case RETRO_HW_CONTEXT_OPENGLES2:
        return "OpenGL ES 2";
    case RETRO_HW_CONTEXT_OPENGL_CORE:
        return "OpenGL core " + version;
    case RETRO_HW_CONTEXT_OPENGLES3:
        return "OpenGL ES 3";
    case RETRO_HW_CONTEXT_OPENGLES_VERSION:
        return "OpenGL ES " + version;
    case RETRO_HW_CONTEXT_D3D9:
        return "Direct3D 9";
    case RETRO_HW_CONTEXT_D3D10:
        return "Direct3D 10";
    case RETRO_HW_CONTEXT_D3D11:
        return "Direct3D 11";
    case RETRO_HW_CONTEXT_D3D12:
        return "Direct3D 12";
    default:
        return "hardware context " + std::to_string(static_cast<int>(callback.context_type));
    }
}

HardwareRender& of(void* self) {
    return *static_cast<HardwareRender*>(self);
}

} // namespace

bool HardwareRender::setRender(const retro_hw_render_callback& callback) {
    if (callback.context_type != RETRO_HW_CONTEXT_VULKAN) {
        refusal_ = "it asks for " + contextName(callback) + ", and openSU draws with Vulkan only";
        lucent::error("retro", "refused the core's {}: openSU draws with Vulkan only",
                      contextName(callback));
        return false;
    }
    callback_ = callback;
    requested_ = true;
    refusal_.clear();
    lucent::info("retro", "the core draws with Vulkan");
    return true;
}

bool HardwareRender::setNegotiation(
    const retro_hw_render_context_negotiation_interface& negotiation) {
    if (negotiation.interface_type != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN) {
        return false;
    }
    negotiation_ =
        reinterpret_cast<const retro_hw_render_context_negotiation_interface_vulkan*>(&negotiation);
    lucent::info("retro", "the core negotiates its Vulkan device (interface version {})",
                 negotiation_->interface_version);
    return true;
}

void HardwareRender::negotiationSupport(retro_hw_render_context_negotiation_interface& query) {
    query.interface_version =
        query.interface_type == RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN
            ? RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION
            : 0;
}

bool HardwareRender::renderInterface(const retro_hw_render_interface** out) const {
    if (presenter_ == nullptr) {
        return false;
    }
    *out = reinterpret_cast<const retro_hw_render_interface*>(&interface_);
    return true;
}

void HardwareRender::attach(Presenter& presenter) {
    if (!requested_) {
        return;
    }
    presenter_ = &presenter;
    const VulkanDevice& device = presenter.device();
    interface_ = retro_hw_render_interface_vulkan{
        .interface_type = RETRO_HW_RENDER_INTERFACE_VULKAN,
        .interface_version = RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION,
        .handle = this,
        .instance = presenter.instance(),
        .gpu = device.physical(),
        .device = device.handle(),
        .get_device_proc_addr = vkGetDeviceProcAddr,
        .get_instance_proc_addr = vkGetInstanceProcAddr,
        .queue = device.queue(),
        .queue_index = device.queueFamily(),
        .set_image = setImage,
        .get_sync_index = syncIndex,
        .get_sync_index_mask = syncIndexMask,
        .set_command_buffers = setCommandBuffers,
        .wait_sync_index = waitSyncIndex,
        .lock_queue = lockQueue,
        .unlock_queue = unlockQueue,
        .set_signal_semaphore = setSignalSemaphore,
    };
    if (callback_.context_reset != nullptr) {
        callback_.context_reset();
    }
    contextUp_ = true;
}

void HardwareRender::detach() {
    if (!contextUp_) {
        return;
    }
    presenter_->device().waitIdle();
    if (callback_.context_destroy != nullptr) {
        callback_.context_destroy();
    }
    contextUp_ = false;
    presenter_ = nullptr;
    image_.reset();
    shown_.reset();
    imageWaits_.clear();
    pending_ = {};
}

void HardwareRender::frameReady(unsigned width, unsigned height) {
    if (!image_) {
        lucent::warn("retro", "the core sent a Vulkan frame without an image");
        return;
    }
    shown_ = image_;
    width_ = width;
    height_ = height;
    pending_.waits.insert(pending_.waits.end(), imageWaits_.begin(), imageWaits_.end());
    imageWaits_.clear();
    fresh_ = true;
}

bool HardwareRender::takeFresh() {
    return std::exchange(fresh_, false);
}

std::optional<FrameImage> HardwareRender::frame(float aspect) const {
    if (!shown_) {
        return std::nullopt;
    }
    return FrameImage{
        .rgba = {}, .width = width_, .height = height_, .aspect = aspect, .gpu = shown_};
}

CoreSync HardwareRender::takeSync() {
    CoreSync sync = std::exchange(pending_, {});
    // Command buffers handed over carry their own barriers; their semaphores are not waited.
    if (!sync.commands.empty()) {
        sync.waits.clear();
    }
    return sync;
}

void HardwareRender::setImage(void* self, const retro_vulkan_image* image, std::uint32_t count,
                              const VkSemaphore* semaphores, std::uint32_t queueFamily) {
    HardwareRender& render = of(self);
    render.image_ = CoreImage{
        .image = image->create_info.image,
        .view = image->image_view,
        .format = image->create_info.format,
        .layout = image->image_layout,
        .queueFamily = queueFamily,
    };
    render.imageWaits_.assign(semaphores, semaphores + count);
}

std::uint32_t HardwareRender::syncIndex(void* /*self*/) {
    // One frame in flight: the presenter waits for the last one before drawing the next.
    return 0;
}

std::uint32_t HardwareRender::syncIndexMask(void* /*self*/) {
    return 1;
}

void HardwareRender::setCommandBuffers(void* self, std::uint32_t count,
                                       const VkCommandBuffer* buffers) {
    CoreSync& pending = of(self).pending_;
    pending.commands.insert(pending.commands.end(), buffers, buffers + count);
}

void HardwareRender::waitSyncIndex(void* self) {
    const VkResult waited = of(self).presenter_->waitForFrame();
    if (waited != VK_SUCCESS) {
        lucent::error("retro", "waiting for the last frame failed: VkResult {}",
                      static_cast<int>(waited));
    }
}

void HardwareRender::lockQueue(void* self) {
    of(self).presenter_->device().lockQueue();
}

void HardwareRender::unlockQueue(void* self) {
    of(self).presenter_->device().unlockQueue();
}

void HardwareRender::setSignalSemaphore(void* self, VkSemaphore semaphore) {
    of(self).pending_.signal = semaphore;
}

} // namespace opensu::retro
