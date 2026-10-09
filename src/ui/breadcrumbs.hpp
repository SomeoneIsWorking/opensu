// breadcrumbs — the trail that names where the player is, and its geometry: a glass pill at the
// top bar's start, one cell per level with a chevron between. opensu's own; iiSU names no place.
// Pure, so the geometry is tested without a window; `app::trailOf` builds the trail from the
// shell's state and `BreadcrumbPainter` draws it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"
#include "library/sections.hpp"

namespace opensu::ui {

/// What a level of the trail is, and so what a click on it goes back to.
enum class CrumbKind : std::uint8_t {
    /// A dock section: Home or Library.
    Section,
    /// The folder open in Library (a store, All games or a console).
    Folder,
    /// The search whose results are showing.
    Search,
    /// A filter narrowing the view; not a place, so not clickable.
    Filter,
    /// The game whose details page is open.
    Game,
    /// The Settings screen.
    Settings,
    /// The Devices page.
    Devices,
    /// The category of the Settings screen or the tab of the Devices page on show; the page's own
    /// level, not a place of its own.
    Page,
};

struct Crumb {
    std::string label;
    CrumbKind kind{CrumbKind::Section};
    /// The section a `Section` crumb stands for.
    library::Section section{library::Section::Home};

    bool operator==(const Crumb&) const = default;
};

using Trail = std::vector<Crumb>;

/// Whether `crumb` names a place a click can return to; a filter and the open game are not.
[[nodiscard]] bool isPlace(const Crumb& crumb) noexcept;

/// What the geometry is made from, in pixels.
struct BreadcrumbFrame {
    /// The pill's left edge and top, its height, and the right edge it must stay left of.
    float left{};
    float top{};
    float height{};
    float maxRight{};
    /// Pixels per dp.
    float dp{1.0f};
};

struct BreadcrumbLayout {
    Rect bar;
    /// Each crumb's cell, text included.
    std::vector<Rect> cells;
    /// Each crumb's text width, after fitting.
    std::vector<float> textWidths;
    /// The centre of the chevron after each crumb but the last.
    std::vector<float> chevrons;

    /// The crumb under the point, or nothing.
    [[nodiscard]] std::optional<std::size_t> crumbAt(float x, float y) const noexcept;
};

/// Lays `textWidths`, the measured width of each label, out in `frame`. When the trail would pass
/// `maxRight`, the longest labels give up width first so every level stays visible.
[[nodiscard]] BreadcrumbLayout layoutBreadcrumbs(const BreadcrumbFrame& frame,
                                                 const std::vector<float>& textWidths);

/// The sizes the layout and the painter share, in dp.
inline constexpr float crumbPadDp = 14.0f;
inline constexpr float crumbCellPadDp = 6.0f;
inline constexpr float crumbChevronDp = 16.0f;
inline constexpr float crumbMinTextDp = 28.0f;

} // namespace opensu::ui
