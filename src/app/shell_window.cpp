#include "shell_window.hpp"

#include <algorithm>
#include <utility>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>

#include "gpu2d/gpu_error.hpp"
#include "gpu2d/render_target.hpp"

#include "lucent/log.h"
#include "sdl_keys.hpp"
#include "session/monitor.hpp"

namespace opensu::app {
namespace {

/// The layout is computed from the window's size, but a window smaller than this crowds it.
constexpr int minimumWidth = 960;
constexpr int minimumHeight = 600;
constexpr std::chrono::nanoseconds frameInterval{1'000'000'000 / 60};

gpu2d::WindowOptions windowOptions(const ShellWindow::Options& options) {
    return gpu2d::WindowOptions{.title = options.title,
                                .width = options.width,
                                .height = options.height,
                                .hidden = options.hidden};
}

SDL_GPUCommandBuffer* acquireCommands(SDL_GPUDevice& device) {
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(&device);
    if (commands == nullptr) {
        throw gpu2d::GpuError{"cannot acquire a command buffer"};
    }
    return commands;
}

} // namespace

ShellWindow::ShellWindow(const Options& options)
    : window_{windowOptions(options)}, canvas_{window_.device()} {
    SDL_SetWindowMinimumSize(window_.window(), minimumWidth, minimumHeight);
    SDL_StartTextInput(window_.window());
    lucent::info("shell", "drawing on {} through {}, {} window", window_.deviceName(),
                 window_.driver(), window_.translucent() ? "translucent" : "opaque");
}

std::optional<session::Output> ShellWindow::output() const {
    return session::displayOutput(SDL_GetDisplayForWindow(window_.window()));
}

gpu2d::PixelSize ShellWindow::fillMonitor() {
    if (const std::optional<session::Output> monitor = output()) {
        SDL_SetWindowSize(window_.window(), monitor->width, monitor->height);
        SDL_SyncWindow(window_.window());
    }
    return size();
}

void ShellWindow::setShown(bool shown) {
    if (shown) {
        SDL_ShowWindow(window_.window());
    } else {
        SDL_HideWindow(window_.window());
    }
}

bool ShellWindow::pollEvents() {
    keys_.nextFrame();
    pointer_ = PointerFrame{};
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        handle(event);
    }
    postStroke();
    if (SDL_GetMouseFocus() == window_.window()) {
        float x = 0.0f;
        float y = 0.0f;
        SDL_GetMouseState(&x, &y);
        pointer_.point = ui::Point{x, y};
    }
    return !closing_;
}

void ShellWindow::handle(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        closing_ = true;
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        resized_ = gpu2d::PixelSize{event.window.data1, event.window.data2};
        break;
    case SDL_EVENT_KEY_DOWN:
        if (const std::optional<int> key = keyOf(event.key.scancode)) {
            keys_.keyDown(*key, event.key.repeat);
        }
        break;
    case SDL_EVENT_KEY_UP:
        if (const std::optional<int> key = keyOf(event.key.scancode)) {
            keys_.keyUp(*key);
        }
        break;
    case SDL_EVENT_TEXT_INPUT:
        keys_.type(event.text.text);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        pointer_.delta.x += event.motion.xrel;
        pointer_.delta.y += event.motion.yrel;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        pointer_.left = pointer_.left || event.button.button == SDL_BUTTON_LEFT;
        pointer_.right = pointer_.right || event.button.button == SDL_BUTTON_RIGHT;
        pointer_.middle = pointer_.middle || event.button.button == SDL_BUTTON_MIDDLE;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        pointer_.wheel +=
            event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
        break;
    default:
        break;
    }
}

void ShellWindow::press(input::Combo combo) {
    Stroke stroke;
    if (combo.ctrl) {
        stroke.keys.push_back(SDL_SCANCODE_LCTRL);
    }
    if (combo.shift) {
        stroke.keys.push_back(SDL_SCANCODE_LSHIFT);
    }
    const std::optional<SDL_Scancode> key = scancodeOf(combo.key);
    if (!key) {
        lucent::warn("shell", "no key gives code {}, so it is not pressed", combo.key);
        return;
    }
    stroke.keys.push_back(*key);
    strokes_.push_back(std::move(stroke));
}

void ShellWindow::type(std::string text) {
    strokes_.push_back(Stroke{.keys = {}, .text = std::move(text)});
}

void ShellWindow::postStroke() {
    if (strokes_.empty()) {
        return;
    }
    const Stroke& stroke = strokes_.front();
    if (strokeDown_) {
        for (auto key = stroke.keys.rbegin(); key != stroke.keys.rend(); ++key) {
            postKey(*key, false);
        }
        strokes_.pop_front();
        strokeDown_ = false;
        return;
    }
    for (const SDL_Scancode key : stroke.keys) {
        postKey(key, true);
    }
    if (!stroke.text.empty()) {
        SDL_Event event{};
        event.text.type = SDL_EVENT_TEXT_INPUT;
        event.text.windowID = SDL_GetWindowID(window_.window());
        event.text.text = stroke.text.c_str();
        post(event);
    }
    strokeDown_ = true;
}

void ShellWindow::postKey(SDL_Scancode key, bool down) {
    SDL_Event event{};
    event.key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = SDL_GetWindowID(window_.window());
    event.key.scancode = key;
    event.key.key = SDL_GetKeyFromScancode(key, SDL_KMOD_NONE, false);
    event.key.down = down;
    post(event);
}

void ShellWindow::post(SDL_Event& event) {
    if (!SDL_PushEvent(&event)) {
        lucent::error("shell", "could not post an injected event: {}", SDL_GetError());
    }
}

std::optional<gpu2d::PixelSize> ShellWindow::takeResize() noexcept {
    return std::exchange(resized_, std::nullopt);
}

void ShellWindow::submit(SDL_GPUCommandBuffer* commands) {
    canvas_.submit(commands);
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        throw gpu2d::GpuError{"cannot submit a frame"};
    }
}

void ShellWindow::present(const Draw& draw) {
    SDL_GPUCommandBuffer* commands = acquireCommands(device());
    if (const std::optional<gpu2d::Surface> surface = window_.acquire(commands)) {
        draw(canvas_, *surface);
    }
    submit(commands);
    pace();
}

gpu2d::image::Bitmap ShellWindow::render(const Draw& draw) {
    const gpu2d::PixelSize frame = size();
    const gpu2d::RenderTarget target{device(), frame.width, frame.height};
    draw(canvas_, target.surface());
    submit(acquireCommands(device()));
    return target.read();
}

void ShellWindow::pace() {
    const auto now = std::chrono::steady_clock::now();
    if (now < nextFrame_) {
        SDL_DelayPrecise(static_cast<Uint64>((nextFrame_ - now).count()));
    }
    nextFrame_ = std::max(nextFrame_ + frameInterval, std::chrono::steady_clock::now());
}

} // namespace opensu::app
