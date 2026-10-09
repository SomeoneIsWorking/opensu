// glass — the HUD's glass pill body (iiSU ea3.n with its fill from ea3.e), and the elevated pill
// the dock stands on (iiSU `ia3.Pill`, navigation.md §1.3).
#pragma once

#include "home_layout.hpp"

namespace opensu::ui {

/// How an elevated glass pill is drawn.
struct PillStyle {
    /// Dark or light variant, by the chrome's theme (iiSU `ya0.e`).
    bool dark{false};
    /// The border's width in pixels.
    float borderWidth{};
    /// Pixels per dp, which the shadow's dp sizes scale by.
    float dp{1.0f};
    /// Everything is drawn at this opacity, for a pill that fades.
    float alpha{1.0f};
};

class GlassPainter {
  public:
    /// A fully rounded pill over `body`, in the light theme's glass fill.
    void paint(const Rect& body) const;
    /// `body` with corners of `radius` pixels.
    void paint(const Rect& body, float radius) const;
    /// A capsule over `body` with the shadow, fill, tint and border of iiSU's `ia3.Pill`.
    void paintPill(const Rect& body, const PillStyle& style) const;
};

} // namespace opensu::ui
