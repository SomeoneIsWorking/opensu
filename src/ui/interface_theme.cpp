#include "interface_theme.hpp"

#include <algorithm>

namespace opensu::ui {

float fingerDp(Density density) noexcept {
    return std::max(density.dp, density.display);
}

gpu2d::Colour colourOf(theme::Colour colour) noexcept {
    return gpu2d::Colour{colour.r, colour.g, colour.b, colour.a};
}

gpu2d::ui::Theme interfaceTheme(Density density, const theme::InterfaceTheme& tokens) noexcept {
    gpu2d::ui::Theme theme;
    const theme::Palette& from = tokens.palette;
    gpu2d::ui::Palette& colours = theme.colours;
    colours.ground = colourOf(from.ground);
    colours.surface = colourOf(from.surface);
    colours.control = colourOf(from.control);
    colours.controlFocused = colourOf(from.controlFocused);
    colours.selected = colourOf(from.selected);
    colours.ink = colourOf(from.ink);
    colours.inkMuted = colourOf(from.inkMuted);
    colours.inkFaint = colourOf(from.inkFaint);
    colours.accent = colourOf(from.accent);
    colours.onAccent = colourOf(from.onAccent);
    colours.track = colourOf(from.track);
    colours.focusRing = colourOf(from.focusRing);
    colours.scrim = colourOf(from.scrim);
    colours.toast = colourOf(from.toast);
    colours.toastError = colourOf(from.toastError);
    colours.onToast = colourOf(from.onToast);
    colours.danger = colourOf(from.danger);

    const theme::Spacing& lengths = tokens.spacing;
    gpu2d::ui::Spacing& spacing = theme.spacing;
    spacing.gap = lengths.gap;
    spacing.pad = lengths.pad;
    spacing.section = lengths.section;
    spacing.rowHeight = lengths.rowHeight;
    spacing.compactRowHeight = lengths.compactRowHeight;
    spacing.sidebarWidth = lengths.sidebarWidth;
    spacing.contentWidth = lengths.contentWidth;
    spacing.hintBarHeight = lengths.hintBarHeight;
    spacing.dialogWidth = lengths.dialogWidth;
    const float finger = fingerDp(density) / density.dp;
    spacing.touchTarget *= finger;
    spacing.touchSlop *= finger;

    gpu2d::ui::Shape& shape = theme.shape;
    shape.radius = tokens.shape.radius;
    shape.smallRadius = tokens.shape.smallRadius;
    shape.outline = tokens.shape.outline;
    shape.focusRing = tokens.shape.focusRing;
    shape.focusGap = tokens.shape.focusGap;

    gpu2d::ui::TypeScale& type = theme.type;
    type.title = tokens.type.title;
    type.heading = tokens.type.heading;
    type.body = tokens.type.body;
    type.caption = tokens.type.caption;
    type.value = tokens.type.value;
    type.hint = tokens.type.hint;
    type.toast = tokens.type.toast;

    theme.scale = density.dp;
    return theme;
}

} // namespace opensu::ui
