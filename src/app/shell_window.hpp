// shell_window — the shell's SDL window on gpu2d: the window and GPU device, the canvas the shell
// draws on, a frame to the window or to an offscreen target, and the window's events as the
// frame's keyboard, pointer, resize and close. Keys and text injected over the control channel are
// posted to the same event queue, so they reach the shell exactly as the keyboard's do.
#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>

#include "gpu2d/canvas.hpp"
#include "gpu2d/gpu_window.hpp"
#include "gpu2d/image/bitmap.hpp"
#include "gpu2d/surface.hpp"

#include "input/key_names.hpp"
#include "input/keyboard_bindings.hpp"
#include "session/gamescope.hpp"
#include "ui/paint.hpp"

namespace opensu::app {

/// The pointer as one frame saw it.
struct PointerFrame {
    /// The pointer inside the window, or nothing.
    std::optional<ui::Point> point;
    /// How far it moved since the last frame.
    ui::Point delta{};
    /// Wheel notches this frame: positive is away from the player.
    float wheel{};
    /// Buttons that went down this frame.
    bool left{false};
    bool right{false};
    bool middle{false};
};

class ShellWindow {
  public:
    struct Options {
        std::string title;
        int width{};
        int height{};
        /// Unmapped and without vsync, for maintainer runs and still renders.
        bool hidden{false};
    };

    /// Records a frame into a target on the canvas.
    using Draw = std::function<void(gpu2d::Canvas& canvas, const gpu2d::Surface& target)>;

    /// Opens the window, its GPU device and the canvas; throws gpu2d::GpuError when it cannot.
    explicit ShellWindow(const Options& options);

    [[nodiscard]] SDL_GPUDevice& device() const noexcept {
        return window_.device();
    }
    [[nodiscard]] gpu2d::PixelSize size() const {
        return window_.pixelSize();
    }
    /// The X11 window id, which Gamescope's overlay property names.
    [[nodiscard]] std::uint64_t x11Window() const {
        return window_.x11Window();
    }
    /// The size and refresh rate of the monitor the window is on.
    [[nodiscard]] std::optional<session::Output> output() const;

    /// Covers the window's monitor; returns the size it now has.
    gpu2d::PixelSize fillMonitor();
    /// Maps or unmaps the window.
    void setShown(bool shown);

    /// Takes the window's events since the last call; false once the window was asked to close.
    [[nodiscard]] bool pollEvents();
    /// The keyboard and text the last `pollEvents` gave.
    [[nodiscard]] const input::KeyFrame& keys() const noexcept {
        return keys_;
    }
    /// The pointer as the last `pollEvents` left it.
    [[nodiscard]] const PointerFrame& pointer() const noexcept {
        return pointer_;
    }
    /// Posts `combo` to the event queue as the keyboard presses it: down in one frame, up in the
    /// next, after whatever was posted before it.
    void press(input::Combo combo);
    /// Posts `text` to the event queue as the keyboard types it, after whatever was posted before.
    void type(std::string text);

    /// The window's new size, once per resize.
    [[nodiscard]] std::optional<gpu2d::PixelSize> takeResize() noexcept;

    /// Draws one frame into the window and presents it, at most 60 a second.
    void present(const Draw& draw);
    /// Draws one frame the window's size into an offscreen target and reads it back
    /// (premultiplied RGBA).
    [[nodiscard]] gpu2d::image::Bitmap render(const Draw& draw);

  private:
    /// A key combination or text waiting to be posted.
    struct Stroke {
        /// Modifiers first, then the key.
        std::vector<SDL_Scancode> keys;
        std::string text;
    };

    /// Posts the next step of the oldest stroke: its keys down and its text, or its keys up.
    void postStroke();
    void postKey(SDL_Scancode key, bool down);
    void post(SDL_Event& event);
    /// Encodes the canvas's frame into `commands` and submits it.
    void submit(SDL_GPUCommandBuffer* commands);
    void handle(const SDL_Event& event);
    /// Waits out what is left of the frame's 60th of a second.
    void pace();

    gpu2d::GpuWindow window_;
    gpu2d::Canvas canvas_;
    input::KeyFrame keys_;
    PointerFrame pointer_;
    std::optional<gpu2d::PixelSize> resized_;
    /// Strokes to post; the front one's keys are down while `strokeDown_`. Its text stays here
    /// until the event naming it was taken.
    std::deque<Stroke> strokes_;
    bool strokeDown_{false};
    bool closing_{false};
    std::chrono::steady_clock::time_point nextFrame_{std::chrono::steady_clock::now()};
};

} // namespace opensu::app
