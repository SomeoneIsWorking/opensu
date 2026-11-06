#include "shell_window.hpp"

#include <algorithm>
#include <utility>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_touch.h>

#include "gpu2d/gpu_error.hpp"
#include "gpu2d/render_target.hpp"

#include "display/output.hpp"
#include "lucent/log.h"
#include "sdl_keys.hpp"
#include "touch/sdl_touch.hpp"

namespace opensu::app {
namespace {

/// `button`'s bit in `PointerFrame::held`, none for a button past the third.
unsigned heldBit(std::uint8_t button) {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return 1U;
    case SDL_BUTTON_MIDDLE:
        return 2U;
    case SDL_BUTTON_RIGHT:
        return 4U;
    default:
        return 0U;
    }
}

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
    // The system's on-screen keyboard shows only when a field asks for it (`showScreenKeyboard`).
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    SDL_StartTextInput(window_.window());
    lucent::info("shell", "drawing on {} through {}, {} window, {}", window_.deviceName(),
                 window_.driver(), window_.translucent() ? "translucent" : "opaque",
                 window_.importsDmabuf() ? "DMA-BUFs sampled" : "no DMA-BUF import");
    SDL_Rect safe{};
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window_.window(), &width, &height);
    SDL_GetWindowSafeArea(window_.window(), &safe);
    lucent::info("shell", "window {}x{}, safe area {}x{} at {},{}, {} screen keyboard", width,
                 height, safe.w, safe.h, safe.x, safe.y,
                 SDL_HasScreenKeyboardSupport() ? "a" : "no");
}

std::optional<display::Output> ShellWindow::output() const {
    return display::displayOutput(SDL_GetDisplayForWindow(window_.window()));
}

gpu2d::PixelSize ShellWindow::fillMonitor() {
    if (const std::optional<display::Output> monitor = output()) {
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

void ShellWindow::raise() {
    SDL_RestoreWindow(window_.window());
    SDL_RaiseWindow(window_.window());
}

bool ShellWindow::hasTouchscreen() const noexcept {
    int count = 0;
    const SDL_TouchID* devices = SDL_GetTouchDevices(&count);
    SDL_free(const_cast<SDL_TouchID*>(devices));
    return count > 0;
}

touch::WindowSurface ShellWindow::touchSurface() const {
    const gpu2d::PixelSize window = size();
    return touch::surfaceOf(window_.window(), static_cast<unsigned>(window.width),
                            static_cast<unsigned>(window.height));
}

bool ShellWindow::hasScreenKeyboard() const noexcept {
    return SDL_HasScreenKeyboardSupport();
}

void ShellWindow::showScreenKeyboard(bool shown) {
    if (shown == screenKeyboard_) {
        return;
    }
    screenKeyboard_ = shown;
    // SDL reads the hint when text input starts, so input restarts under the new one.
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, shown ? "1" : "0");
    SDL_StopTextInput(window_.window());
    SDL_StartTextInput(window_.window());
    lucent::info("shell", "system keyboard {}", shown ? "shown" : "hidden");
}

void ShellWindow::lower() {
    SDL_MinimizeWindow(window_.window());
}

bool ShellWindow::pollEvents() {
    keys_.nextFrame();
    pointer_ = PointerFrame{};
    fingers_.clear();
    systemBack_ = false;
    screenKeyboardHidden_ = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        handle(event);
    }
    postStroke();
    pointer_.held = pointerHeld_;
    // An event's own position stands, as a posted one never moves SDL's mouse.
    if (!pointer_.point && SDL_GetMouseFocus() == window_.window()) {
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
        systemBack_ =
            systemBack_ || (event.key.scancode == SDL_SCANCODE_AC_BACK && !event.key.repeat);
        if (const std::optional<int> key = keyOf(event.key.scancode)) {
            keys_.keyDown(*key, event.key.repeat);
        }
        if (event.key.scancode <= SDL_SCANCODE_RGUI &&
            std::ranges::find(keysHeld_, event.key.scancode) == keysHeld_.end()) {
            keysHeld_.push_back(static_cast<std::uint8_t>(event.key.scancode));
        }
        break;
    case SDL_EVENT_KEY_UP:
        if (const std::optional<int> key = keyOf(event.key.scancode)) {
            keys_.keyUp(*key);
        }
        if (event.key.scancode <= SDL_SCANCODE_RGUI) {
            std::erase(keysHeld_, static_cast<std::uint8_t>(event.key.scancode));
        }
        break;
    case SDL_EVENT_TEXT_INPUT:
        keys_.type(event.text.text);
        break;
    case SDL_EVENT_SCREEN_KEYBOARD_HIDDEN:
        screenKeyboardHidden_ = screenKeyboard_;
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        fingers_.push_back(touch::fingerOf(window_.window(), event.tfinger));
        break;
    case SDL_EVENT_MOUSE_MOTION:
        pointer_.point = ui::Point{event.motion.x, event.motion.y};
        pointer_.delta.x += event.motion.xrel;
        pointer_.delta.y += event.motion.yrel;
        pointer_.touch = event.motion.which == SDL_TOUCH_MOUSEID;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        pointer_.point = ui::Point{event.button.x, event.button.y};
        if (event.button.button == SDL_BUTTON_LEFT) {
            pointer_.primary.push_back(false);
        }
        pointerHeld_ = static_cast<std::uint8_t>(pointerHeld_ & ~heldBit(event.button.button));
        pointer_.touch = event.button.which == SDL_TOUCH_MOUSEID;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        pointer_.point = ui::Point{event.button.x, event.button.y};
        if (event.button.button == SDL_BUTTON_LEFT) {
            pointer_.primary.push_back(true);
        }
        pointerHeld_ = static_cast<std::uint8_t>(pointerHeld_ | heldBit(event.button.button));
        pointer_.touch = event.button.which == SDL_TOUCH_MOUSEID;
        pointer_.left = pointer_.left || event.button.button == SDL_BUTTON_LEFT;
        pointer_.right = pointer_.right || event.button.button == SDL_BUTTON_RIGHT;
        pointer_.middle = pointer_.middle || event.button.button == SDL_BUTTON_MIDDLE;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        pointer_.point = ui::Point{event.wheel.mouse_x, event.wheel.mouse_y};
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

void ShellWindow::movePointer(ui::Point at) {
    SDL_Event event{};
    event.motion.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(window_.window());
    event.motion.x = at.x;
    event.motion.y = at.y;
    // A posted move always moves, so it hovers even onto the spot the pointer was last put.
    event.motion.xrel = 1.0f;
    event.motion.yrel = 1.0f;
    post(event);
}

void ShellWindow::pressPointer(ui::Point at, std::uint8_t button, bool down) {
    SDL_Event event{};
    event.button.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(window_.window());
    event.button.button = button;
    event.button.down = down;
    event.button.clicks = 1;
    event.button.x = at.x;
    event.button.y = at.y;
    post(event);
}

void ShellWindow::turnWheel(ui::Point at, float notches) {
    SDL_Event event{};
    event.wheel.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.windowID = SDL_GetWindowID(window_.window());
    event.wheel.y = notches;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    event.wheel.mouse_x = at.x;
    event.wheel.mouse_y = at.y;
    post(event);
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

std::string ShellWindow::clipboard() const {
    char* held = SDL_GetClipboardText();
    if (held == nullptr) {
        return {};
    }
    std::string text{held};
    SDL_free(held);
    return text;
}

void ShellWindow::setClipboard(const std::string& text) {
    if (!SDL_SetClipboardText(text.c_str())) {
        lucent::error("shell", "could not set the clipboard: {}", SDL_GetError());
    }
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

float ShellWindow::displayDp() const noexcept {
    const float scale = SDL_GetWindowDisplayScale(window_.window());
    return scale > 0.0f ? scale : 1.0f;
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

ShellWindow::Presented ShellWindow::present(const Draw& draw) {
    SDL_GPUCommandBuffer* commands = acquireCommands(device());
    const std::optional<gpu2d::Surface> surface = window_.acquire(commands);
    if (surface) {
        draw(canvas_, *surface);
    }
    submit(commands);
    const Presented presented{.submitted = std::chrono::steady_clock::now(),
                              .drawn = surface.has_value()};
    pace();
    return presented;
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
