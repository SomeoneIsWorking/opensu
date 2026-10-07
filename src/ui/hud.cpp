#include "hud.hpp"

#include <algorithm>

#include "typeface.hpp"

namespace iideck::ui {
namespace {

void fillRounded(Rectangle rect, float roundness, Color colour) {
    DrawRectangleRounded(rect, roundness, 12, colour);
}

// The prompt row sits this many units above the bottom edge, its caps this tall.
constexpr float promptBaselineUnits = 3.0f;
constexpr float promptTextUnits = 1.1f;
constexpr float promptCapPaddingUnits = 0.9f;

} // namespace

void Hud::setSize(int width, int height) noexcept {
    width_ = width;
    height_ = height;
}

float Hud::unit() const noexcept {
    return static_cast<float>(std::min(width_, height_)) / 100.0f;
}

Rectangle Hud::topPillRect() const noexcept {
    const float u = unit();
    const float height = u * 6.0f;
    const float width = static_cast<float>(width_) * 0.42f;
    return Rectangle{(static_cast<float>(width_) - width) / 2.0f, u * 1.5f, width, height};
}

float Hud::topInset() const noexcept {
    const Rectangle pill = topPillRect();
    return pill.y + pill.height;
}

float Hud::bottomInset() const noexcept {
    return unit() * (promptBaselineUnits + promptTextUnits + promptCapPaddingUnits);
}

void Hud::setToast(std::string text, bool isError, Clock::time_point now) {
    toast_ = std::move(text);
    toastError_ = isError;
    toastUntil_ = now + toastLifetime;
}

void Hud::tick(Clock::time_point now) {
    if (!toast_.empty() && now >= toastUntil_) {
        toast_.clear();
    }
}

void Hud::drawGround() const {
    ClearBackground(palette::ground);
    const float u = unit();
    const float spacing = std::max(u * 2.2f, 8.0f);
    const float radius = std::max(spacing * 0.09f, 1.0f);
    const Color dot{palette::ink.r, palette::ink.g, palette::ink.b, 24};
    for (float y = spacing; y < static_cast<float>(height_); y += spacing) {
        for (float x = spacing; x < static_cast<float>(width_); x += spacing) {
            DrawCircleV({x, y}, radius, dot);
        }
    }
}

void Hud::drawTopBar(const std::string& focusedTitle) const {
    const float u = unit();
    const Rectangle pill = topPillRect();
    fillRounded(pill, 0.5f, Color{0x7c, 0x5c, 0xff, 0x24});
    DrawRectangleRoundedLinesEx(pill, 0.5f, 12, std::max(u * 0.12f, 1.0f),
                                Color{0x7c, 0x5c, 0xff, 0x40});

    if (!focusedTitle.empty()) {
        const int size = static_cast<int>(u * 2.2f);
        const float textWidth = type().measure(focusedTitle, size);
        type().draw(focusedTitle.c_str(), pill.x + (pill.width - textWidth) / 2.0f,
                    pill.y + (pill.height - static_cast<float>(size)) / 2.0f, size, palette::ink);
    }

    const float baseline = pill.y + pill.height / 2.0f - u * 0.9f;
    const int size = static_cast<int>(u * 1.6f);
    if (!status_.empty()) {
        type().draw(status_.c_str(), u, baseline, size, palette::inkSoft);
    }
    float right = static_cast<float>(width_) - u;
    if (!clock_.empty()) {
        const float clockWidth = type().measure(clock_, size);
        type().draw(clock_.c_str(), right - clockWidth, baseline, size, palette::inkSoft);
        right -= clockWidth + u * 1.6f;
    }
    drawServiceStatus(right, pill.y + pill.height / 2.0f);
}

void Hud::drawServiceStatus(float right, float centreY) const {
    if (steamState_ == ServiceState::Hidden) {
        return;
    }
    const float u = unit();
    Color dot = palette::dotReady;
    const char* label = "ready";
    switch (steamState_) {
    case ServiceState::Starting:
        dot = palette::dotWorking;
        label = "starting";
        break;
    case ServiceState::Failed:
        dot = palette::dotFailed;
        label = "failed";
        break;
    case ServiceState::Blocked:
        dot = palette::dotFailed;
        label = "on desktop";
        break;
    case ServiceState::Hidden:
    case ServiceState::Ready:
        break;
    }

    const int size = static_cast<int>(u * 1.5f);
    const float labelWidth = type().measure(label, size);
    type().draw(label, right - labelWidth, centreY - static_cast<float>(size) / 2.0f, size,
                palette::inkSoft);

    const float dotRadius = u * 0.35f;
    const float dotX = right - labelWidth - u * 0.6f - dotRadius;
    DrawCircleV({dotX, centreY}, dotRadius, dot);

    // A generic client glyph: a ring with a dot inside.
    const float glyphRadius = u * 0.9f;
    const Vector2 glyph{dotX - dotRadius - u * 0.7f - glyphRadius, centreY};
    DrawRing(glyph, glyphRadius * 0.78f, glyphRadius, 0.0f, 360.0f, 32, palette::inkSoft);
    DrawCircleV(glyph, glyphRadius * 0.34f, palette::inkSoft);
}

void Hud::drawPrompts() const {
    const float u = unit();
    const float baseline = static_cast<float>(height_) - u * promptBaselineUnits;
    const int size = static_cast<int>(u * promptTextUnits);
    // A rounded key cap followed by the label.
    const auto prompt = [&](const char* key, const char* label, float x) {
        const float capWidth = type().measure(key, size) + u * 1.2f;
        const float capHeight = static_cast<float>(size) + u * promptCapPaddingUnits;
        fillRounded(Rectangle{x, baseline - capHeight, capWidth, capHeight}, 0.4f, palette::panel);
        type().draw(key, x + u * 0.6f, baseline - u * 0.6f, size, palette::ink);
        type().draw(label, x + capWidth + u * 0.6f, baseline - u * 0.6f, size, palette::inkSoft);
    };
    prompt("A", "Play", u * 1.4f);
    const float rightWidth = type().measure("Refresh", size);
    prompt("X", "Refresh", static_cast<float>(width_) - u * 1.4f - rightWidth - u * 2.4f);
}

void Hud::drawToast() const {
    if (toast_.empty()) {
        return;
    }
    const float u = unit();
    const int size = static_cast<int>(u * 1.4f);
    const float textWidth = type().measure(toast_, size);
    const float padding = u * 2.0f;
    const Rectangle box{
        (static_cast<float>(width_) - textWidth - 2.0f * padding) / 2.0f,
        static_cast<float>(height_) - u * 8.0f,
        textWidth + 2.0f * padding,
        static_cast<float>(size) + padding,
    };
    fillRounded(box, 0.5f,
                toastError_ ? Color{0xb3, 0x26, 0x1e, 240} : Color{0x2b, 0x27, 0x33, 240});
    type().draw(toast_.c_str(), box.x + padding / 2.0f, box.y + padding / 3.0f, size, WHITE);
}

} // namespace iideck::ui
