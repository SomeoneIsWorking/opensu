#include "touch/touch_layer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "touch/pad_layout.hpp"

namespace opensu::touch {
namespace {

using gamepad::Button;

/// In assets, beside the touch art.
constexpr std::string_view fontName = "CalSans-Regular.ttf";

std::vector<Button> eightWay(int sector) {
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

std::vector<Button> stickButtons(const Stick& stick, float x, float y) {
    const float dx = x - stick.x;
    const float dy = y - stick.y;
    if (std::hypot(dx, dy) < stick.deadZone) {
        return {};
    }
    if (stick.ways == 2) {
        if (stick.vertical) {
            return {dy < 0.0F ? Button::Up : Button::Down};
        }
        return {dx < 0.0F ? Button::Left : Button::Right};
    }
    constexpr float eighth = std::numbers::pi_v<float> / 4.0F;
    const float angle = std::atan2(dy, dx);
    if (stick.ways == 4) {
        return eightWay((static_cast<int>(std::lround(angle / (eighth * 2.0F))) * 2) & 7);
    }
    // A diagonal holds both of its directions.
    return eightWay(static_cast<int>(std::lround(angle / eighth)) & 7);
}

} // namespace

TouchLayer::TouchLayer(std::filesystem::path art)
    : fontFile_{art.parent_path() / fontName}, art_{std::move(art)} {
}

void TouchLayer::setPanel(ArcadePanel panel) {
    panel_ = std::move(panel);
    if (!labels_) {
        labels_ = std::make_unique<LabelFace>(fontFile_);
    }
}

void TouchLayer::offerRotate() {
    rotate_ = true;
}

void TouchLayer::setSize(float size) {
    size_ = size;
}

void TouchLayer::layout(const WindowSurface& surface, const Box& picture) {
    touches_.clear();
    width_ = surface.width;
    height_ = surface.height;
    WindowSurface sized = surface;
    sized.dp *= size_;
    plan_ = panel_ ? arcadeLayout(sized, picture, *panel_, art_, *labels_)
                   : padLayout(sized, art_, rotate_);
    dirty_ = true;
}

std::vector<ControlPlace> TouchLayer::places() const {
    std::vector<ControlPlace> places;
    for (const Control& control : plan_.controls) {
        if (control.stick) {
            places.push_back(
                {Button::None, control.stick->x, control.stick->y, control.hit.width * 0.5F});
        }
        for (const Target& target : control.targets) {
            places.push_back({target.button, target.x, target.y, 0.0F});
        }
    }
    return places;
}

std::optional<std::size_t> TouchLayer::controlAt(float x, float y) const {
    for (std::size_t at = 0; at < plan_.controls.size(); ++at) {
        const Control& control = plan_.controls[at];
        const Box& hit = control.hit;
        const bool inside =
            control.round ? std::hypot(x - hit.centreX(), y - hit.centreY()) <= hit.width * 0.5F
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
    const auto nearest =
        std::ranges::min_element(control.targets, {}, [x, y](const Target& target) {
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
            touches_.insert_or_assign(finger.id, Touch{*control, buttonsAt(plan_.controls[*control],
                                                                           finger.x, finger.y)});
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

std::pair<float, float> TouchLayer::leaning(float distance) const {
    const float across =
        (pressed(Button::Right) ? 1.0F : 0.0F) - (pressed(Button::Left) ? 1.0F : 0.0F);
    const float along = (pressed(Button::Down) ? 1.0F : 0.0F) - (pressed(Button::Up) ? 1.0F : 0.0F);
    const float length = std::hypot(across, along);
    if (length == 0.0F) {
        return {0.0F, 0.0F};
    }
    return {across * distance / length, along * distance / length};
}

void TouchLayer::draw() {
    pixels_.assign(static_cast<std::size_t>(width_) * height_ * 4, 0);
    for (const Piece& piece : plan_.pieces) {
        const bool down = std::ranges::any_of(piece.lit, [this](Button button) {
            return pressed(button);
        });
        const Art& shown = down && !piece.down.rgba.empty() ? piece.down : piece.art;
        const auto [leanX, leanY] = leaning(piece.lean);
        composeOver(pixels_, static_cast<int>(width_), static_cast<int>(height_), shown,
                    ArtSpot{.left = static_cast<int>(std::lround(piece.box.x + leanX)),
                            .top = static_cast<int>(std::lround(piece.box.y + leanY)),
                            .opacity = down ? 1.0F : piece.idle});
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
