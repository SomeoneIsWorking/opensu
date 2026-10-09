// launcher_badges — the launchers opensu depends on, as icons in the top bar's friends slot:
// each launcher's logo in an iiSU avatar circle (a32.e), with a presence dot for its state.
#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "raylib.h"

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
    ServiceState state{ServiceState::Hidden};
    /// A download the launcher is running, 0 to 1.
    std::optional<double> progress;

    bool operator==(const LauncherBadge&) const = default;
};

/// Where the badges go, in pixels.
struct BadgeRow {
    /// The first badge's left edge and the row's vertical centre.
    Vector2 start{};
    float diameter{};
    float gap{};
    /// Drives the starting spinner.
    double seconds{};
};

class LauncherBadgePainter {
  public:
    /// Draws the visible badges left to right along `row`.
    void paint(std::span<const LauncherBadge> badges, const BadgeRow& row);

  private:
    void paintOne(const LauncherBadge& badge, const BadgeRow& row, Vector2 centre);

    IconAtlas icons_;
};

} // namespace opensu::ui
