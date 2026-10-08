// dock_painter — draws iiSU's primary navigation bar (`gh3.K`): a glass capsule at the bottom
// centre holding one icon per section, with the LB and RB badges on its top corners.
#pragma once

#include <span>

#include "raylib.h"

#include "button_glyph.hpp"
#include "dock_metrics.hpp"
#include "glass.hpp"

namespace iideck::ui {

/// One section's icon as it is drawn now.
struct DockIcon {
    /// The icon's texture, or null while the APK's file has not arrived.
    const Texture* texture{nullptr};
    /// The selected pop (1 to 1.06) about the icon's centre.
    float scale{1.0f};
    /// What stands in for the icon until its texture arrives: the section's initial.
    const char* initial{""};
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
    /// The bar where this frame draws it, slid down by how far it has left.
    [[nodiscard]] static Rect barRect(const DockLayout& layout, const DockStyle& style) noexcept;

    /// Draws the bar of `layout`, `icons` in item order.
    void paint(const DockLayout& layout, const DockMetrics& metrics,
               std::span<const DockIcon> icons, const DockStyle& style);

  private:
    [[nodiscard]] static float slideOffset(const DockLayout& layout,
                                           const DockStyle& style) noexcept;

    GlassPainter glass_;
    ButtonGlyphPainter glyphs_;
};

} // namespace iideck::ui
