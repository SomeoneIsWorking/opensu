// launcher_badges — the launchers opensu depends on, as icons inline in the top bar's status pill
// where iiSU has its bell: each launcher's logo with a presence dot for its state. The status pill
// painter draws them with the clock.
#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "raylib.h"

#include "home_layout.hpp"
#include "library/game.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

/// How a launcher is doing, as its badge shows it.
enum class ServiceState : std::uint8_t {
    /// Not on this machine or not in use; no badge.
    Hidden,
    Starting,
    Ready,
    /// Unusable until the player acts, such as signing in, or broken.
    Failed,
    /// Something outside opensu holds it.
    Blocked,
};

struct LauncherBadge {
    Icon icon{Icon::Steam};
    /// The store it stands for, which a click opens.
    library::Source source{library::Source::Steam};
    ServiceState state{ServiceState::Hidden};
    /// A download the launcher is running, 0 to 1.
    std::optional<double> progress;

    bool operator==(const LauncherBadge&) const = default;
};

/// Where the badges go, in pixels.
struct BadgeRow {
    /// One circle's bounds per visible badge, in order (`TopBarLayout::launchers`).
    std::span<const Rect> cells;
    /// The launcher the pointer is on.
    std::optional<library::Source> hovered;
    /// Drives the starting spinner.
    double seconds{};
};

class LauncherBadgePainter {
  public:
    explicit LauncherBadgePainter(IconAtlas& icons) noexcept : icons_{icons} {
    }

    /// Draws the visible badges, each in its cell of `row`.
    void paint(std::span<const LauncherBadge> badges, const BadgeRow& row);

  private:
    void paintOne(const LauncherBadge& badge, const BadgeRow& row, const Rect& cell);

    IconAtlas& icons_;
};

} // namespace opensu::ui
