#include "sdl_runner_window.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include "lucent/log.h"
#include "touch/sdl_touch.hpp"

namespace opensu::retro {

SdlRunnerWindow::SdlRunnerWindow(const Options& options) : hidden_{options.hidden} {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        throw std::runtime_error{std::string{"video: "} + SDL_GetError()};
    }
    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;
    if (!options.hidden) {
        flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
        if (options.fullscreen) {
            flags |= SDL_WINDOW_FULLSCREEN;
        }
    }
    window_ = SDL_CreateWindow("openSU", options.width, options.height, flags);
    if (window_ == nullptr) {
        const std::string reason = SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        throw std::runtime_error{"window: " + reason};
    }
}

SdlRunnerWindow::~SdlRunnerWindow() {
    SDL_DestroyWindow(window_);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

std::span<const char* const> SdlRunnerWindow::surfaceExtensions() const {
    Uint32 count = 0;
    const char* const* names = SDL_Vulkan_GetInstanceExtensions(&count);
    if (names == nullptr) {
        throw std::runtime_error{std::string{"Vulkan window: "} + SDL_GetError()};
    }
    return {names, count};
}

VkSurfaceKHR SdlRunnerWindow::openSurface(VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window_, instance, nullptr, &surface)) {
        throw std::runtime_error{std::string{"Vulkan surface: "} + SDL_GetError()};
    }
    return surface;
}

VkSurfaceKHR SdlRunnerWindow::makeSurface(VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window_, instance, nullptr, &surface)) {
        return VK_NULL_HANDLE;
    }
    return surface;
}

void SdlRunnerWindow::resize(unsigned width, unsigned height) {
    if (!SDL_SetWindowFullscreen(window_, false) ||
        !SDL_SetWindowSize(window_, static_cast<int>(width), static_cast<int>(height))) {
        lucent::warn("retro", "the window stays its size: {}", SDL_GetError());
    }
}

void SdlRunnerWindow::destroySurface(VkInstance instance, VkSurfaceKHR surface) {
    SDL_Vulkan_DestroySurface(instance, surface, nullptr);
}

WindowInput SdlRunnerWindow::poll() {
    WindowInput input;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            input.quit = true;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            input.resized = true;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            input.focused = false;
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            input.focused = true;
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            input.fingers.push_back(touch::fingerOf(window_, event.tfinger));
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (!event.key.repeat) {
                input.keys.push_back(
                    {static_cast<int>(event.key.scancode), event.type == SDL_EVENT_KEY_DOWN});
            }
            break;
        default:
            break;
        }
    }
    return input;
}

bool SdlRunnerWindow::hasSurface() const {
    return hidden_ ||
           SDL_GetPointerProperty(SDL_GetWindowProperties(window_),
                                  SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr) != nullptr;
}

VkExtent2D SdlRunnerWindow::pixels() const {
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window_, &width, &height);
    return {static_cast<std::uint32_t>(std::max(width, 0)),
            static_cast<std::uint32_t>(std::max(height, 0))};
}

touch::WindowSurface SdlRunnerWindow::surface(unsigned width, unsigned height) const {
    return touch::surfaceOf(window_, width, height);
}

void SdlRunnerWindow::inject(const touch::Finger& finger) {
    SDL_Event event = touch::eventOf(window_, finger);
    if (!SDL_PushEvent(&event)) {
        lucent::warn("retro", "a finger was not queued: {}", SDL_GetError());
    }
}

} // namespace opensu::retro
