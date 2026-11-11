// shell_window — the shell's SDL window on gpu2d: the window and GPU device, the canvas the shell
// draws on, a frame to the window or to an offscreen target, and the window's events as the
// frame's keyboard, pointer, resize and close. Keys, text and pointer events injected over the
// control channel are posted to the same event queue, so they reach the shell exactly as the
// keyboard's and mouse's do.
#pragma once

#include <atomic>
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
#include "gpu2d/ui/widget.hpp"

#include "display/output.hpp"
#include "input/key_names.hpp"
#include "input/keyboard_bindings.hpp"
#include "touch/touch_surface.hpp"
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
    /// Buttons held as the frame ends: bit 0 left, 1 middle, 2 right.
    std::uint8_t held{0};
    /// The primary button's or the finger's changes this frame, in order: true down, false up.
    std::vector<bool> primary;
    /// This frame's pointer events came from a finger (SDL's touch mouse).
    bool touch{false};
};

class ShellWindow {
  public:
    struct Options {
        std::string title;
        /// The size: dp for a window on a display, pixels for a hidden one.
        int width{};
        int height{};
        /// Unmapped and without vsync, for maintainer runs and still renders.
        bool hidden{false};
        /// Hidden only: pixels per dp of the display it stands for (`Config::hiddenDisplayScale`).
        float hiddenDp{1.0F};
    };

    /// Records a frame into a target on the canvas.
    using Draw = std::function<void(gpu2d::Canvas& canvas, const gpu2d::Surface& target)>;

    /// Opens the window, its GPU device and the canvas; throws gpu2d::GpuError when it cannot.
    explicit ShellWindow(const Options& options);
    ~ShellWindow();
    ShellWindow(const ShellWindow&) = delete;
    ShellWindow& operator=(const ShellWindow&) = delete;
    ShellWindow(ShellWindow&&) = delete;
    ShellWindow& operator=(ShellWindow&&) = delete;

    [[nodiscard]] SDL_GPUDevice& device() const noexcept {
        return window_.device();
    }
    /// Whether what the window leaves clear shows what lies below it.
    [[nodiscard]] bool translucent() const noexcept {
        return window_.translucent();
    }
    /// Whether the device samples DMA-BUFs (`gpu2d::DmabufPicture`).
    [[nodiscard]] bool importsDmabuf() const noexcept {
        return window_.importsDmabuf();
    }
    [[nodiscard]] gpu2d::PixelSize size() const {
        return window_.pixelSize();
    }
    /// The X11 window id, which Gamescope's overlay property names.
    [[nodiscard]] std::uint64_t x11Window() const {
        return window_.x11Window();
    }
    /// The size and refresh rate of the monitor the window is on.
    [[nodiscard]] std::optional<display::Output> output() const;

    /// Covers the window's monitor; returns the size it now has.
    gpu2d::PixelSize fillMonitor();
    /// Maps or unmaps the window.
    void setShown(bool shown);
    /// Puts the window in front and gives it the focus, restoring it when it was lowered.
    void raise();
    /// Minimizes the window, so whatever is behind it comes to the front.
    void lower();

    /// Takes the window's events since the last call; false once the window was asked to close.
    [[nodiscard]] bool pollEvents();
    /// The keyboard and text the last `pollEvents` gave.
    [[nodiscard]] const input::KeyFrame& keys() const noexcept {
        return keys_;
    }
    /// Every physical key held now, as SDL's scancodes (USB HID usages), oldest first.
    [[nodiscard]] const std::vector<std::uint8_t>& keysHeld() const noexcept {
        return keysHeld_;
    }
    /// The pointer as the last `pollEvents` left it.
    [[nodiscard]] const PointerFrame& pointer() const noexcept {
        return pointer_;
    }
    /// The system's own Back (Android's gesture or button) came this frame.
    [[nodiscard]] bool systemBack() const noexcept {
        return systemBack_;
    }
    /// What the fingers did this frame, oldest first.
    [[nodiscard]] const std::vector<touch::Finger>& fingers() const noexcept {
        return fingers_;
    }
    /// Whether this machine has a touchscreen.
    [[nodiscard]] bool hasTouchscreen() const noexcept;
    /// The window as an on-screen pad covers it.
    [[nodiscard]] touch::WindowSurface touchSurface() const;
    /// Pixels in from each edge the interface keeps clear of (`contentInsets`).
    [[nodiscard]] gpu2d::ui::Insets contentInsets() const;
    /// Whether the system has an on-screen keyboard (Android's) a text field can ask for.
    [[nodiscard]] bool hasScreenKeyboard() const noexcept;
    /// Shows or hides the system's on-screen keyboard; it types through the same text events.
    void showScreenKeyboard(bool shown);
    /// The system's on-screen keyboard was put away this frame, by the player or the system.
    [[nodiscard]] bool screenKeyboardHidden() const noexcept {
        return screenKeyboardHidden_;
    }
    /// The system clipboard's text; empty when it holds none.
    [[nodiscard]] std::string clipboard() const;
    /// Puts `text` on the system clipboard.
    void setClipboard(const std::string& text);
    /// Posts `combo` to the event queue as the keyboard presses it: down in one frame, up in the
    /// next, after whatever was posted before it.
    void press(input::Combo combo);
    /// Posts `text` to the event queue as the keyboard types it, after whatever was posted before.
    void type(std::string text);
    /// Posts the mouse moving to `at` in window pixels.
    void movePointer(ui::Point at);
    /// Posts the mouse's `button` (an `SDL_BUTTON_*`) going down or up at `at`.
    void pressPointer(ui::Point at, std::uint8_t button, bool down);
    /// Posts what SDL makes of a finger's tap at `at`.
    void tapPointer(ui::Point at);
    /// Posts the wheel turning `notches` at `at`, positive away from the player.
    void turnWheel(ui::Point at, float notches);

    /// Pixels per dp of the display the window is on: SDL's display scale, 1 when it has none.
    [[nodiscard]] float displayDp() const noexcept;

    /// The window's new size, once per resize.
    [[nodiscard]] std::optional<gpu2d::PixelSize> takeResize() noexcept;
    /// Whether the app came back to the foreground (Android's resume), once per return.
    [[nodiscard]] bool takeForeground() noexcept;

    /// A frame `present` submitted: when, and whether the window had a surface to draw it into
    /// (a hidden window has none).
    struct Presented {
        std::chrono::steady_clock::time_point submitted;
        bool drawn{false};
    };
    /// How often the shell presents unless told otherwise.
    static constexpr int shellHz = 60;
    /// Draws one frame into the window and presents it, at most one each `presentEvery`.
    Presented present(const Draw& draw);
    /// Presents at most one frame each `interval` from now on.
    void presentEvery(std::chrono::nanoseconds interval) noexcept {
        interval_ = interval;
    }
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
    /// Waits out what is left of the frame's interval.
    void pace();

    gpu2d::GpuWindow window_;
    gpu2d::Canvas canvas_;
    input::KeyFrame keys_;
    std::vector<std::uint8_t> keysHeld_;
    std::vector<touch::Finger> fingers_;
    PointerFrame pointer_;
    /// The mouse's buttons held, as `PointerFrame::held`; kept across frames.
    std::uint8_t pointerHeld_{0};
    std::optional<gpu2d::PixelSize> resized_;
    /// A hidden window's stated pixels per dp; none for a window on a display.
    std::optional<float> hiddenDp_;
    /// SDL hands app lifecycle events only to watchers, on whichever thread sends them.
    static bool watchLifecycle(void* self, SDL_Event* event);
    std::atomic<bool> foreground_{false};
    /// Strokes to post; the front one's keys are down while `strokeDown_`. Its text stays here
    /// until the event naming it was taken.
    std::deque<Stroke> strokes_;
    bool strokeDown_{false};
    bool closing_{false};
    bool systemBack_{false};
    bool screenKeyboard_{false};
    bool screenKeyboardHidden_{false};
    std::chrono::steady_clock::time_point nextFrame_{std::chrono::steady_clock::now()};
    std::chrono::nanoseconds interval_{std::chrono::nanoseconds{std::chrono::seconds{1}} / shellHz};
};

} // namespace opensu::app
