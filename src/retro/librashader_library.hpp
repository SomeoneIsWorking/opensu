// librashader_library — the librashader C API (`librashader.h`, MIT) loaded at run time from the
// file openSU stages beside the runner (`Config::librashader`), by explicit path: the header's own
// loader, `librashader_ld.h`, opens the bare name `librashader.so` from the loader's search path,
// which would find a system copy or none. Only the Vulkan runtime and the preset calls the chain
// uses are bound; a missing file, symbol or ABI mismatch refuses with the reason.
#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

#define LIBRA_RUNTIME_VULKAN
#include <librashader.h>

namespace opensu::retro {

/// A shader preset that cannot run; the message says why, for the player.
class ShaderError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class Librashader {
  public:
    /// Loads `file`; throws ShaderError when it is missing, incomplete or of another ABI.
    explicit Librashader(const std::filesystem::path& file);
    Librashader(const Librashader&) = delete;
    Librashader& operator=(const Librashader&) = delete;
    Librashader(Librashader&&) = delete;
    Librashader& operator=(Librashader&&) = delete;
    ~Librashader();

    /// The preset in `file`, with the original-aspect uniforms on; throws ShaderError.
    [[nodiscard]] libra_shader_preset_t loadPreset(const std::filesystem::path& file) const;
    void freePreset(libra_shader_preset_t& preset) const;

    /// A filter chain for `preset` (consumed) on `device`; throws ShaderError.
    [[nodiscard]] libra_vk_filter_chain_t makeChain(libra_shader_preset_t& preset,
                                                    const libra_device_vk_t& device,
                                                    const filter_chain_vk_opt_t& options) const;
    void freeChain(libra_vk_filter_chain_t& chain) const;

    /// Records the chain's passes for one frame; throws ShaderError.
    void frame(libra_vk_filter_chain_t& chain, VkCommandBuffer commands, std::size_t count,
               const libra_image_vk_t& input, const libra_image_vk_t& output,
               const libra_viewport_t& viewport, const frame_vk_opt_t& options) const;

  private:
    /// The symbol `name` of the library; throws ShaderError when it is missing.
    [[nodiscard]] void* symbol(const char* name) const;
    /// Throws ShaderError saying `what` failed and why, freeing `error`; nothing for no error.
    void check(libra_error_t error, std::string_view what) const;

    std::filesystem::path file_;
    void* handle_{nullptr};
    PFN_libra_error_write errorWrite_{nullptr};
    PFN_libra_error_free_string errorFreeString_{nullptr};
    PFN_libra_error_free errorFree_{nullptr};
    PFN_libra_preset_create_with_options presetCreate_{nullptr};
    PFN_libra_preset_free presetFree_{nullptr};
    PFN_libra_vk_filter_chain_create chainCreate_{nullptr};
    PFN_libra_vk_filter_chain_frame chainFrame_{nullptr};
    PFN_libra_vk_filter_chain_free chainFree_{nullptr};
};

} // namespace opensu::retro
