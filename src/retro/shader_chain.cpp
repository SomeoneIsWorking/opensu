#include "shader_chain.hpp"

#include "lucent/log.h"

namespace opensu::retro {
namespace {

/// librashader's default reference white for SDR output.
constexpr float sdrWhiteNits = 200.0F;

} // namespace

ShaderChain::ShaderChain(const std::filesystem::path& library, const std::filesystem::path& preset,
                         VkInstance instance, const VulkanDevice& device)
    : library_{library} {
    libra_shader_preset_t loaded = library_.loadPreset(preset);
    const libra_device_vk_t handles{
        .physical_device = device.physical(),
        .instance = instance,
        .device = device.handle(),
        .queue = device.queue(),
        .entry = vkGetInstanceProcAddr,
    };
    filter_chain_vk_opt_t options{};
    options.version = LIBRASHADER_CURRENT_VERSION;
    options.frames_in_flight = 1;
    options.use_dynamic_rendering = false;
    try {
        chain_ = library_.makeChain(loaded, handles, options);
    } catch (...) {
        library_.freePreset(loaded);
        throw;
    }
    library_.freePreset(loaded);
    lucent::info("retro", "shader preset {} built", preset.string());
}

ShaderChain::~ShaderChain() {
    library_.freeChain(chain_);
}

void ShaderChain::record(VkCommandBuffer commands, const ChainImage& input,
                         const ChainImage& output, VkRect2D rect, float aspect) {
    const libra_image_vk_t source{input.image, input.format, input.extent.width,
                                  input.extent.height};
    const libra_image_vk_t target{output.image, output.format, output.extent.width,
                                  output.extent.height};
    const libra_viewport_t viewport{static_cast<float>(rect.offset.x),
                                    static_cast<float>(rect.offset.y), rect.extent.width,
                                    rect.extent.height};
    frame_vk_opt_t options{};
    options.version = LIBRASHADER_CURRENT_VERSION;
    // The history images start undefined; clearing them puts them in a readable layout.
    options.clear_history = frames_ == 0;
    options.frame_direction = 1;
    options.total_subframes = 1;
    options.current_subframe = 1;
    options.aspect_ratio = aspect;
    options.frames_per_second = 1.0F;
    options.brightness_nits = sdrWhiteNits;
    library_.frame(chain_, commands, frames_, source, target, viewport, options);
    ++frames_;
}

} // namespace opensu::retro
