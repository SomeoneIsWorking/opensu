#include "touch/touch_layer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "touch/pad_layout.hpp"

namespace opensu::touch {
namespace {

using gamepad::Button;

constexpr float idleOpacity = 0.65F;

std::vector<Button> stickButtons(const Stick& stick, float x, float y) {
    const float dx = x - stick.x;
    const float dy = y - stick.y;
    if (std::hypot(dx, dy) < stick.deadZone) {
        return {};
    }
    // Eight sectors: a diagonal holds both of its directions.
    const auto sector =
        static_cast<int>(std::lround(std::atan2(dy, dx) / (std::numbers::pi_v<float> / 4.0F))) & 7;
    switch (sector) {
    case 0:
        return {Button::Right};
    case 1:
        return {Button::Right, Button::Down};
    case 2:
        return {Button::Down};
    case 3:
        return {Button::Down, Button::Left};
    case 4:
        return {Button::Left};
    case 5:
        return {Button::Left, Button::Up};
    case 6:
        return {Button::Up};
    default:
        return {Button::Up, Button::Right};
    }
}

} // namespace

TouchLayer::TouchLayer(std::filesystem::path art) : art_{std::move(art)} {
}

void TouchLayer::layout(const WindowSurface& surface) {
    touches_.clear();
    width_ = surface.width;
    height_ = surface.height;
    plan_ = padLayout(surface, art_);
    dirty_ = true;
}

std::optional<std::size_t> TouchLayer::controlAt(float x, float y) const {
    for (std::size_t at = 0; at < plan_.controls.size(); ++at) {
        const Control& control = plan_.controls[at];
        const Box& hit = control.hit;
        const bool inside =
            control.round
                ? std::hypot(x - hit.centreX(), y - hit.centreY()) <= hit.width * 0.5F
                : hit.contains(x, y);
        if (inside) {
            return at;
        }
    }
    return std::nullopt;
}

std::vector<Button> TouchLayer::buttonsAt(const Control& control, float x, float y) const {
    if (control.stick) {
        return stickButtons(*control.stick, x, y);
    }
    const auto nearest = std::ranges::min_element(control.targets, {}, [x, y](const Target& target) {
        return std::hypot(x - target.x, y - target.y);
    });
    if (nearest == control.targets.end()) {
        return {};
    }
    return {nearest->button};
}

bool TouchLayer::finger(const Finger& finger) {
    const std::vector<Button> before = held();
    switch (finger.phase) {
    case Finger::Phase::Down:
        if (const std::optional<std::size_t> control = controlAt(finger.x, finger.y)) {
            touches_.insert_or_assign(
                finger.id,
                Touch{*control, buttonsAt(plan_.controls[*control], finger.x, finger.y)});
        }
        break;
    case Finger::Phase::Move:
        if (auto touch = touches_.find(finger.id); touch != touches_.end()) {
            touch->second.buttons =
                buttonsAt(plan_.controls[touch->second.control], finger.x, finger.y);
        }
        break;
    case Finger::Phase::Up:
        touches_.erase(finger.id);
        break;
    }
    const bool changed = held() != before;
    dirty_ = dirty_ || changed;
    return changed;
}

bool TouchLayer::releaseAll() {
    const bool any = !held().empty();
    touches_.clear();
    dirty_ = dirty_ || any;
    return any;
}

std::vector<Button> TouchLayer::held() const {
    std::vector<Button> buttons;
    for (const auto& [id, touch] : touches_) {
        for (const Button button : touch.buttons) {
            if (std::ranges::find(buttons, button) == buttons.end()) {
                buttons.push_back(button);
            }
        }
    }
    std::ranges::sort(buttons);
    return buttons;
}

bool TouchLayer::pressed(Button button) const {
    return std::ranges::any_of(touches_, [button](const auto& entry) {
        return std::ranges::find(entry.second.buttons, button) != entry.second.buttons.end();
    });
}

void TouchLayer::compose(const Art& art, const Box& box, float opacity) {
    const int left = static_cast<int>(std::lround(box.x));
    const int top = static_cast<int>(std::lround(box.y));
    for (int row = 0; row < art.height; ++row) {
        const int y = top + row;
        if (y < 0 || y >= static_cast<int>(height_)) {
            continue;
        }
        for (int column = 0; column < art.width; ++column) {
            const int x = left + column;
            if (x < 0 || x >= static_cast<int>(width_)) {
                continue;
            }
            const unsigned char* source =
                &art.rgba[((static_cast<std::size_t>(row) * art.width) + column) * 4];
            const float above = static_cast<float>(source[3]) / 255.0F * opacity;
            if (above <= 0.0F) {
                continue;
            }
            unsigned char* pixel =
                &pixels_[((static_cast<std::size_t>(y) * width_) + static_cast<std::size_t>(x)) *
                         4];
            const float below = static_cast<float>(pixel[3]) / 255.0F;
            const float alpha = above + (below * (1.0F - above));
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const float mixed =
                    ((static_cast<float>(source[channel]) * above) +
                     (static_cast<float>(pixel[channel]) * below * (1.0F - above))) /
                    alpha;
                pixel[channel] = static_cast<unsigned char>(std::lround(mixed));
            }
            pixel[3] = static_cast<unsigned char>(std::lround(alpha * 255.0F));
        }
    }
}

void TouchLayer::draw() {
    pixels_.assign(static_cast<std::size_t>(width_) * height_ * 4, 0);
    for (const Piece& piece : plan_.pieces) {
        const bool down = std::ranges::any_of(piece.lit, [this](Button button) {
            return pressed(button);
        });
        compose(piece.art, piece.box, down ? 1.0F : idleOpacity);
    }
}

OverlayImage TouchLayer::image() {
    const bool changed = std::exchange(dirty_, false);
    if (changed) {
        draw();
    }
    return OverlayImage{pixels_, width_, height_, changed};
}

} // namespace opensu::touch
