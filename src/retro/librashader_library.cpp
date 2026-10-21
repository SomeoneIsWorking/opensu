#include "librashader_library.hpp"

#include <dlfcn.h>

#include <system_error>

#include "lucent/log.h"

namespace opensu::retro {

Librashader::Librashader(const std::filesystem::path& file) : file_{file} {
    std::error_code ec;
    if (file.empty() || !std::filesystem::is_regular_file(file, ec)) {
        throw ShaderError{"shader presets need librashader, which is not installed (" +
                          file.string() + ")"};
    }
    handle_ = dlopen(file.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle_ == nullptr) {
        const char* reason = dlerror();
        throw ShaderError{"librashader cannot be loaded: " +
                          std::string{reason != nullptr ? reason : file.string()}};
    }
    try {
        const auto abi = reinterpret_cast<PFN_libra_instance_abi_version>(
            symbol("libra_instance_abi_version"))();
        const auto api = reinterpret_cast<PFN_libra_instance_api_version>(
            symbol("libra_instance_api_version"))();
        if (abi != LIBRASHADER_CURRENT_ABI || api < LIBRASHADER_CURRENT_VERSION) {
            throw ShaderError{"librashader at " + file.string() + " has ABI " +
                              std::to_string(abi) + " API " + std::to_string(api) +
                              "; the runner needs ABI " + std::to_string(LIBRASHADER_CURRENT_ABI) +
                              " API " + std::to_string(LIBRASHADER_CURRENT_VERSION)};
        }
        errorWrite_ = reinterpret_cast<PFN_libra_error_write>(symbol("libra_error_write"));
        errorFreeString_ =
            reinterpret_cast<PFN_libra_error_free_string>(symbol("libra_error_free_string"));
        errorFree_ = reinterpret_cast<PFN_libra_error_free>(symbol("libra_error_free"));
        presetCreate_ = reinterpret_cast<PFN_libra_preset_create_with_options>(
            symbol("libra_preset_create_with_options"));
        presetFree_ = reinterpret_cast<PFN_libra_preset_free>(symbol("libra_preset_free"));
        chainCreate_ = reinterpret_cast<PFN_libra_vk_filter_chain_create>(
            symbol("libra_vk_filter_chain_create"));
        chainFrame_ = reinterpret_cast<PFN_libra_vk_filter_chain_frame>(
            symbol("libra_vk_filter_chain_frame"));
        chainFree_ =
            reinterpret_cast<PFN_libra_vk_filter_chain_free>(symbol("libra_vk_filter_chain_free"));
    } catch (...) {
        dlclose(handle_);
        throw;
    }
    lucent::info("retro", "librashader ABI {} API {} loaded from {}", LIBRASHADER_CURRENT_ABI,
                 LIBRASHADER_CURRENT_VERSION, file.string());
}

Librashader::~Librashader() {
    dlclose(handle_);
}

void* Librashader::symbol(const char* name) const {
    void* found = dlsym(handle_, name);
    if (found == nullptr) {
        throw ShaderError{"librashader at " + file_.string() + " lacks " + name};
    }
    return found;
}

void Librashader::check(libra_error_t error, std::string_view what) const {
    if (error == nullptr) {
        return;
    }
    char* text = nullptr;
    std::string reason = "unknown error";
    if (errorWrite_(error, &text) == 0 && text != nullptr) {
        reason = text;
        errorFreeString_(&text);
    }
    errorFree_(&error);
    throw ShaderError{std::string{what} + ": " + reason};
}

libra_shader_preset_t Librashader::loadPreset(const std::filesystem::path& file) const {
    libra_preset_opt_t options{};
    options.version = LIBRASHADER_CURRENT_VERSION;
    options.original_aspect_uniforms = true;
    libra_shader_preset_t preset = nullptr;
    check(presetCreate_(file.c_str(), nullptr, &options, &preset),
          "the shader preset " + file.string() + " cannot be read");
    return preset;
}

void Librashader::freePreset(libra_shader_preset_t& preset) const {
    if (preset != nullptr) {
        static_cast<void>(presetFree_(&preset));
    }
}

libra_vk_filter_chain_t Librashader::makeChain(libra_shader_preset_t& preset,
                                               const libra_device_vk_t& device,
                                               const filter_chain_vk_opt_t& options) const {
    libra_vk_filter_chain_t chain = nullptr;
    check(chainCreate_(&preset, device, &options, &chain), "the shader preset cannot be built");
    return chain;
}

void Librashader::freeChain(libra_vk_filter_chain_t& chain) const {
    if (chain != nullptr) {
        static_cast<void>(chainFree_(&chain));
    }
}

void Librashader::frame(libra_vk_filter_chain_t& chain, VkCommandBuffer commands, std::size_t count,
                        const libra_image_vk_t& input, const libra_image_vk_t& output,
                        const libra_viewport_t& viewport, const frame_vk_opt_t& options) const {
    check(chainFrame_(&chain, commands, count, input, output, &viewport, nullptr, &options),
          "the shader preset failed to draw");
}

} // namespace opensu::retro
