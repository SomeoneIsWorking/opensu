// dock_painter — draws iiSU's primary navigation bar (`gh3.K`): a glass capsule at the bottom
// centre holding one icon per section, with the LB and RB badges on its top corners.
#pragma once

#include <span>

#include "paint.hpp"

#include "button_glyph.hpp"
#include "dock_metrics.hpp"
#include "glass.hpp"
#include "library/sections.hpp"
#include "gpu2d/ui/typesetter.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

/// One section's icon as it is drawn now.
struct DockIcon {
    library::Section section{library::Section::Home};
    /// Whether it is the active section, which draws its selected icon.
    bool selected{false};
    /// The selected pop (1 to 1.06) about the icon's centre.
    float scale{1.0f};
    /// Whether the pointer is over the item.
    bool hovered{false};
};

/// How the bar is drawn this frame.
struct DockStyle {
    bool dark{false};
    float dp{1.0f};
    /// 0 (out of view) to 1; the bar slides straight down and fades as it goes.
    float progress{1.0f};
};

class DockPainter {
  public:
    DockPainter(const input::Prompts& prompts, gpu2d::ui::Typesetter& text, IconAtlas& icons) noexcept
        : icons_{icons}, glyphs_{prompts, text} {
    }

    /// The bar where this frame draws it, slid down by how far it has left.
    [[nodiscard]] static Rect barRect(const DockLayout& layout, const DockStyle& style) noexcept;
    /// The highest point this frame draws the dock at, its shoulder badges included.
    [[nodiscard]] static float top(const DockLayout& layout, const DockStyle& style) noexcept;

    /// Draws the bar of `layout`, `icons` in item order.
    void paint(Canvas& canvas, const DockLayout& layout, const DockMetrics& metrics,
               std::span<const DockIcon> icons, const DockStyle& style);

  private:
    [[nodiscard]] static float slideOffset(const DockLayout& layout,
                                           const DockStyle& style) noexcept;

    GlassPainter glass_;
    IconAtlas& icons_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
