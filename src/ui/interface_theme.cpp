#include "interface_theme.hpp"

#include "hud.hpp"

namespace opensu::ui {

gpu2d::ui::Theme interfaceTheme(float dp) noexcept {
    gpu2d::ui::Theme theme;
    gpu2d::ui::Palette& colours = theme.colours;
    colours.ground = palette::ground;
    colours.surface = Colour{0xF8, 0xF8, 0xFB, 255};
    colours.control = palette::panel;
    colours.controlFocused = palette::panel;
    colours.selected = palette::chosen;
    colours.ink = palette::ink;
    colours.inkMuted = palette::inkSoft;
    colours.inkFaint = Colour{0x93, 0x8C, 0xA1, 255};
    colours.accent = palette::control;
    colours.onAccent = palette::panel;
    colours.track = palette::off;
    colours.focusRing = palette::control;
    colours.scrim = Colour{0x2B, 0x27, 0x33, 120};
    colours.toast = Colour{0x2B, 0x27, 0x33, 240};
    colours.toastError = Colour{0xB3, 0x26, 0x1E, 240};
    colours.onToast = palette::panel;
    colours.danger = Colour{0xB3, 0x26, 0x1E, 255};

    gpu2d::ui::Spacing& spacing = theme.spacing;
    spacing.gap = 5.0f;
    spacing.pad = 14.0f;
    spacing.section = 16.0f;
    spacing.rowHeight = 50.0f;
    spacing.compactRowHeight = 40.0f;
    spacing.sidebarWidth = 210.0f;
    spacing.contentWidth = 600.0f;
    spacing.hintBarHeight = 34.0f;
    spacing.dialogWidth = 380.0f;

    gpu2d::ui::Shape& shape = theme.shape;
    shape.radius = 12.0f;
    shape.smallRadius = 9.0f;
    shape.outline = 1.8f;
    shape.focusRing = 2.2f;
    shape.focusGap = 2.0f;

    gpu2d::ui::TypeScale& type = theme.type;
    type.title = 22.0f;
    type.heading = 11.5f;
    type.body = 15.5f;
    type.caption = 11.5f;
    type.value = 14.0f;
    type.hint = 13.0f;
    type.toast = 14.0f;

    theme.scale = dp;
    return theme;
}

} // namespace opensu::ui
