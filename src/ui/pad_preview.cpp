#include "pad_preview.hpp"

#include <algorithm>
#include <cstddef>

namespace opensu::ui {
namespace {

namespace tk = gpu2d::ui;

/// The preview's height, in dp: two stick circles over the trigger bars.
constexpr float heightDp = 220.0f;
constexpr float barDp = 14.0f;
constexpr float dotDp = 9.0f;

// evdev's ABS_X to ABS_RZ.
constexpr std::size_t leftX = 0;
constexpr std::size_t leftY = 1;
constexpr std::size_t leftTrigger = 2;
constexpr std::size_t rightX = 3;
constexpr std::size_t rightY = 4;
constexpr std::size_t rightTrigger = 5;

/// `value` on `range` as -1 to 1.
float centred(std::int32_t value, gamepad::AxisRange range) {
    const float half = (static_cast<float>(range.max) - static_cast<float>(range.min)) * 0.5f;
    if (half <= 0.0f) {
        return 0.0f;
    }
    const float middle = (static_cast<float>(range.max) + static_cast<float>(range.min)) * 0.5f;
    return std::clamp((static_cast<float>(value) - middle) / half, -1.0f, 1.0f);
}

/// `value` on `range` as 0 to 1.
float travel(std::int32_t value, gamepad::AxisRange range) {
    const float span = static_cast<float>(range.max) - static_cast<float>(range.min);
    if (span <= 0.0f) {
        return 0.0f;
    }
    return std::clamp((static_cast<float>(value) - static_cast<float>(range.min)) / span, 0.0f,
                      1.0f);
}

void stick(const tk::Painter& painter, gpu2d::Point centre, float radius, gpu2d::Point played,
           gpu2d::Point read) {
    const tk::Palette& colours = painter.theme.colours;
    const float line = painter.px(painter.theme.shape.outline);
    painter.canvas.fillCircle(centre, radius, painter.fade(colours.control));
    painter.canvas.strokeRoundRect(
        gpu2d::Rect{centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f}, radius,
        line, painter.fade(colours.inkFaint));
    painter.canvas.line({centre.x - radius, centre.y}, {centre.x + radius, centre.y}, line,
                        painter.fade(colours.track));
    painter.canvas.line({centre.x, centre.y - radius}, {centre.x, centre.y + radius}, line,
                        painter.fade(colours.track));
    const float dot = painter.px(dotDp);
    painter.canvas.fillCircle({centre.x + read.x * radius, centre.y + read.y * radius}, dot * 0.7f,
                              painter.fade(colours.inkFaint));
    painter.canvas.fillCircle({centre.x + played.x * radius, centre.y + played.y * radius}, dot,
                              painter.fade(colours.accent));
}

void trigger(const tk::Painter& painter, const gpu2d::Rect& bar, float played, float read) {
    const tk::Palette& colours = painter.theme.colours;
    const float radius = bar.height * 0.5f;
    painter.canvas.fillRoundRect(bar, radius, painter.fade(colours.track));
    if (played > 0.0f) {
        painter.canvas.fillRoundRect(
            gpu2d::Rect{bar.x, bar.y, std::max(bar.height, bar.width * played), bar.height}, radius,
            painter.fade(colours.accent));
    }
    const float at = bar.x + bar.width * read;
    painter.canvas.line({at, bar.y - radius}, {at, bar.y + bar.height + radius},
                        painter.px(painter.theme.shape.outline), painter.fade(colours.ink));
}

} // namespace

tk::Size PadPreview::measure(tk::LayoutContext& context, float maxWidth) {
    return tk::Size{maxWidth, context.theme.px(heightDp)};
}

void PadPreview::paint(tk::Painter& painter) const {
    const gpu2d::Rect& box = frame();
    const float bar = painter.px(barDp);
    const float gap = painter.px(painter.theme.spacing.pad);
    const float radius = std::min(box.width * 0.25f - gap, (box.height - bar - gap * 2.0f) * 0.5f);
    const float y = box.y + radius;
    const gpu2d::Point left{box.x + box.width * 0.25f, y};
    const gpu2d::Point right{box.x + box.width * 0.75f, y};
    const gamepad::RawAxes& raw = axes_.raw;
    const auto played = [](std::int32_t x, std::int32_t y) {
        return gpu2d::Point{centred(x, gamepad::virtualStick), centred(y, gamepad::virtualStick)};
    };
    const auto read = [&raw](std::size_t x, std::size_t y) {
        return gpu2d::Point{centred(raw.value[x], raw.range[x]),
                            centred(raw.value[y], raw.range[y])};
    };
    stick(painter, left, radius, played(axes_.sticks.leftX, axes_.sticks.leftY),
          read(leftX, leftY));
    stick(painter, right, radius, played(axes_.sticks.rightX, axes_.sticks.rightY),
          read(rightX, rightY));
    const float barY = box.y + box.height - bar;
    const float width = radius * 2.0f;
    trigger(painter, gpu2d::Rect{left.x - radius, barY, width, bar},
            travel(axes_.leftTrigger, gamepad::virtualTrigger),
            travel(raw.value[leftTrigger], raw.range[leftTrigger]));
    trigger(painter, gpu2d::Rect{right.x - radius, barY, width, bar},
            travel(axes_.rightTrigger, gamepad::virtualTrigger),
            travel(raw.value[rightTrigger], raw.range[rightTrigger]));
}

} // namespace opensu::ui
