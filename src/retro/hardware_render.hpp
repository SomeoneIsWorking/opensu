// hardware_render — a core drawing with the GPU: SET_HW_RENDER for Vulkan (OpenGL, GLES and
// Direct3D refused, the reason kept), its context negotiation, the retro_hw_render_interface_vulkan
// it is handed (the presenter's instance, device and queue, the queue lock, set_image, the sync
// index), context reset and destroy, and the image of each RETRO_HW_FRAME_BUFFER_VALID frame.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include <libretro.h>
#include <libretro_vulkan.h>

#include "frame_compositor.hpp"
#include "presenter.hpp"

namespace opensu::retro {

class HardwareRender {
  public:
    HardwareRender() = default;
    // The interface the core holds points back here.
    HardwareRender(const HardwareRender&) = delete;
    HardwareRender& operator=(const HardwareRender&) = delete;
    HardwareRender(HardwareRender&&) = delete;
    HardwareRender& operator=(HardwareRender&&) = delete;
    ~HardwareRender() = default;

    /// SET_HW_RENDER: true for Vulkan; any other context is refused and the reason kept.
    bool setRender(const retro_hw_render_callback& callback);
    /// SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE: Vulkan's is kept for the presenter.
    bool setNegotiation(const retro_hw_render_context_negotiation_interface& negotiation);
    /// GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT: version 2 for Vulkan, else 0.
    static void negotiationSupport(retro_hw_render_context_negotiation_interface& query);
    /// GET_HW_RENDER_INTERFACE: false until the context is up.
    bool renderInterface(const retro_hw_render_interface** out) const;

    /// Whether the core draws with Vulkan.
    [[nodiscard]] bool requested() const {
        return requested_;
    }
    [[nodiscard]] const retro_hw_render_context_negotiation_interface_vulkan* negotiation() const {
        return negotiation_;
    }
    /// Why the core's context was refused; empty when none was or Vulkan was taken after.
    [[nodiscard]] const std::string& refusal() const {
        return refusal_;
    }

    /// The core's context is `presenter`'s device: the interface is filled and context_reset
    /// called.
    void attach(Presenter& presenter);
    /// context_destroy, once the GPU is idle; before the game is unloaded.
    void detach();

    /// The core called video refresh with RETRO_HW_FRAME_BUFFER_VALID.
    void frameReady(unsigned width, unsigned height);
    /// Whether a frame arrived since the last call.
    [[nodiscard]] bool takeFresh();
    /// The last valid frame as the compositor draws it; nothing before the first.
    [[nodiscard]] std::optional<FrameImage> frame(float aspect) const;
    /// What the core handed over since the last call, for the next submission.
    [[nodiscard]] CoreSync takeSync();

  private:
    static void setImage(void* self, const retro_vulkan_image* image, std::uint32_t count,
                         const VkSemaphore* semaphores, std::uint32_t queueFamily);
    static std::uint32_t syncIndex(void* self);
    static std::uint32_t syncIndexMask(void* self);
    static void setCommandBuffers(void* self, std::uint32_t count, const VkCommandBuffer* buffers);
    static void waitSyncIndex(void* self);
    static void lockQueue(void* self);
    static void unlockQueue(void* self);
    static void setSignalSemaphore(void* self, VkSemaphore semaphore);

    retro_hw_render_callback callback_{};
    bool requested_{false};
    std::string refusal_;
    const retro_hw_render_context_negotiation_interface_vulkan* negotiation_{nullptr};
    Presenter* presenter_{nullptr};
    retro_hw_render_interface_vulkan interface_{};
    bool contextUp_{false};

    /// The latest set_image and the semaphores that came with it.
    std::optional<CoreImage> image_;
    std::vector<VkSemaphore> imageWaits_;
    /// The image of the last valid frame, which a duplicated frame shows again.
    std::optional<CoreImage> shown_;
    unsigned width_{0};
    unsigned height_{0};
    bool fresh_{false};
    CoreSync pending_;
};

} // namespace opensu::retro
