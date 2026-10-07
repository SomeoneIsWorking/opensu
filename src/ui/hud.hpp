// hud — the chrome drawn around the home grid: ground, top bar, prompts, toast.
//
// iiSU's top bar is only partly recovered (home-grid.md §2), so this keeps
// iideck's own bar rather than inventing one.
#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "raylib.h"

namespace iideck::ui {

/// The HUD palette, as 8-bit-per-channel colours.
struct Colour {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};

    [[nodiscard]] operator Color() const noexcept {
        return Color{r, g, b, a};
    }
};

namespace palette {
inline constexpr Colour ground{0xf4, 0xf3, 0xf7, 255};
inline constexpr Colour panel{0xff, 0xff, 0xff, 255};
inline constexpr Colour ink{0x2b, 0x27, 0x33, 255};
inline constexpr Colour inkSoft{0x6f, 0x68, 0x80, 255};
/// Service status dots: ready, working, and failed.
inline constexpr Colour dotReady{0x3d, 0xdc, 0x84, 255};
inline constexpr Colour dotWorking{0xf2, 0xb1, 0x34, 255};
inline constexpr Colour dotFailed{0xe0, 0x4f, 0x5f, 255};
} // namespace palette

/// How a background service the shell depends on is doing, as the top bar shows it.
enum class ServiceState {
    /// The service is not in use; nothing is drawn.
    Hidden,
    Starting,
    Ready,
    Failed,
    /// Something outside iideck holds the service.
    Blocked,
};

class Hud {
  public:
    using Clock = std::chrono::steady_clock;

    /// How long a toast stays up.
    static constexpr std::chrono::milliseconds toastLifetime{4000};

    void setSize(int width, int height) noexcept;

    /// Height the top bar takes from the grid, in pixels.
    [[nodiscard]] float topInset() const noexcept;
    /// Height the prompt row takes from the grid, in pixels.
    [[nodiscard]] float bottomInset() const noexcept;

    void setStatus(std::string text) {
        status_ = std::move(text);
    }
    void setClock(std::string text) {
        clock_ = std::move(text);
    }
    void setSteamState(ServiceState state) noexcept {
        steamState_ = state;
    }
    void setToast(std::string text, bool isError, Clock::time_point now);
    /// Clears a toast whose time is up.
    void tick(Clock::time_point now);

    [[nodiscard]] const std::string& status() const noexcept {
        return status_;
    }
    [[nodiscard]] const std::string& toast() const noexcept {
        return toast_;
    }
    [[nodiscard]] bool toastIsError() const noexcept {
        return toastError_;
    }

    void drawGround() const;
    /// The top bar, with the focused title in its centre pill.
    void drawTopBar(const std::string& focusedTitle) const;
    void drawPrompts() const;
    void drawToast() const;

  private:
    /// One hundredth of the window's shorter side, the HUD's base unit.
    [[nodiscard]] float unit() const noexcept;
    [[nodiscard]] Rectangle topPillRect() const noexcept;
    void drawServiceStatus(float right, float centreY) const;

    int width_{};
    int height_{};
    std::string status_;
    std::string clock_;
    ServiceState steamState_{ServiceState::Hidden};
    std::string toast_;
    bool toastError_{false};
    Clock::time_point toastUntil_{};
};

} // namespace iideck::ui
