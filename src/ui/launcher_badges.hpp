// launcher_badges — the launchers opensu depends on, as icons inline in the top bar's status pill
// where iiSU has its bell: each launcher's logo with a presence dot for its state, then the
// Downloads badge while anything downloads, ringed with its progress. The status pill painter
// draws them with the clock.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "paint.hpp"

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
    /// The store it stands for, which a click opens; none for the Downloads badge, which opens the
    /// Downloads page.
    std::optional<library::Source> source{library::Source::Steam};
    ServiceState state{ServiceState::Hidden};
    /// How far what it downloads is, 0 to 1.
    std::optional<double> progress;

    bool operator==(const LauncherBadge&) const = default;
};

/// Where the badges go, in pixels.
struct BadgeRow {
    /// One circle's bounds per visible badge, in order (`TopBarLayout::launchers`).
    std::span<const Rect> cells;
    /// The visible badge the pointer is on, by its place in the row.
    std::optional<std::size_t> hovered;
    /// Drives the starting spinner.
    double seconds{};
};

class LauncherBadgePainter {
  public:
    explicit LauncherBadgePainter(IconAtlas& icons) noexcept : icons_{icons} {
    }

    /// Draws the visible badges, each in its cell of `row`.
    void paint(Canvas& canvas, std::span<const LauncherBadge> badges, const BadgeRow& row);

  private:
    void paintOne(Canvas& canvas, const LauncherBadge& badge, const BadgeRow& row,
                  std::size_t cell);

    IconAtlas& icons_;
};

} // namespace opensu::ui
