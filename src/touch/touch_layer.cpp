#include "touch/touch_layer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

#include "touch/console_layout.hpp"

namespace opensu::touch {
namespace {

using gamepad::Button;

/// In assets, beside the touch art.
constexpr std::string_view fontName = "CalSans-Regular.ttf";

/// `eighths` of a turn from east as a sector 0 to 7.
int sectorOf(long eighths) {
    return static_cast<int>(((eighths % 8) + 8) % 8);
}

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

/// How far a finger at `x`, `y` leans analog `stick` across and down, each from -1 to 1.
std::pair<float, float> stickLean(const Stick& stick, float x, float y) {
    const float dx = x - stick.x;
    const float dy = y - stick.y;
    const float length = std::hypot(dx, dy);
    if (length < stick.deadZone || stick.reach <= stick.deadZone) {
        return {0.0F, 0.0F};
    }
    if (stick.ways == 2) {
        const float along = stick.vertical ? dy : dx;
        const float share = std::clamp(
            (std::abs(along) - stick.deadZone) / (stick.reach - stick.deadZone), 0.0F, 1.0F);
        const float towards = along < 0.0F ? -share : share;
        return stick.vertical ? std::pair{0.0F, towards} : std::pair{towards, 0.0F};
    }
    if (stick.ways == 4 || stick.ways == 8) {
        const float step = 2.0F * std::numbers::pi_v<float> / static_cast<float>(stick.ways);
        const float angle = std::round(std::atan2(dy, dx) / step) * step;
        return {std::cos(angle), std::sin(angle)};
    }
    const float share =
        std::min((length - stick.deadZone) / (stick.reach - stick.deadZone), 1.0F) / length;
    return {dx * share, dy * share};
}

std::int32_t onStickScale(float lean) {
    return static_cast<std::int32_t>(
        std::lround(lean * static_cast<float>(gamepad::virtualStick.max)));
}

std::vector<Button> stickButtons(const Stick& stick, float x, float y) {
    if (stick.analog) {
        return {};
    }
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
        return eightWay(sectorOf(std::lround(angle / (eighth * 2.0F)) * 2));
    }
    // A diagonal holds both of its directions.
    return eightWay(sectorOf(std::lround(angle / eighth)));
}

} // namespace

TouchLayer::TouchLayer(std::filesystem::path art)
    : fontFile_{art.parent_path() / fontName}, art_{std::move(art)} {
}

void TouchLayer::setPanel(ArcadePanel panel) {
    panel_ = std::move(panel);
}

void TouchLayer::clearPanel() {
    panel_.reset();
}

void TouchLayer::setLayout(const LayoutWish& wish) {
    wish_ = wish;
}

LabelFace& TouchLayer::labels() {
    if (!labels_) {
        labels_ = std::make_unique<LabelFace>(fontFile_);
    }
    return *labels_;
}

void TouchLayer::offerRotate() {
    rotate_ = true;
}

void TouchLayer::setSize(float size) {
    size_ = size;
}

void TouchLayer::layout(const WindowSurface& surface, const Box& picture) {
    touches_.clear();
    look_ = {};
    dp_ = surface.dp;
    width_ = surface.width;
    height_ = surface.height;
    WindowSurface sized = surface;
    sized.dp *= size_;
    plan_ = panel_ ? arcadeLayout(sized, picture, *panel_, art_, labels(), rotate_)
                   : consoleLayout(sized, wish_, art_, labels(), rotate_);
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
    const Leans wasLeaning = leans();
    switch (finger.phase) {
    case Finger::Phase::Down:
        if (const std::optional<std::size_t> control = controlAt(finger.x, finger.y)) {
            touches_.insert_or_assign(
                finger.id, Touch{*control, buttonsAt(plan_.controls[*control], finger.x, finger.y),
                                 finger.x, finger.y});
        }
        break;
    case Finger::Phase::Move:
        if (auto touch = touches_.find(finger.id); touch != touches_.end()) {
            if (plan_.controls[touch->second.control].look) {
                look_.first += finger.x - touch->second.x;
                look_.second += finger.y - touch->second.y;
            }
            touch->second.buttons =
                buttonsAt(plan_.controls[touch->second.control], finger.x, finger.y);
            touch->second.x = finger.x;
            touch->second.y = finger.y;
        }
        break;
    case Finger::Phase::Up:
        touches_.erase(finger.id);
        break;
    }
    const bool changed = held() != before || leans() != wasLeaning;
    dirty_ = dirty_ || changed;
    return changed;
}

std::pair<float, float> TouchLayer::takeLook() {
    const std::pair<float, float> moved{look_.first / dp_, look_.second / dp_};
    look_ = {};
    return moved;
}

bool TouchLayer::releaseAll() {
    const bool any = !held().empty() || sticks() != gamepad::Sticks{};
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

TouchLayer::Leans TouchLayer::leans() const {
    Leans out{};
    for (const auto& [id, touch] : touches_) {
        const std::optional<Stick>& stick = plan_.controls[touch.control].stick;
        if (!stick || !stick->analog) {
            continue;
        }
        const auto [across, down] = stickLean(*stick, touch.x, touch.y);
        auto& side = out.at(*stick->analog == StickSide::Left ? 0 : 1);
        if (std::hypot(across, down) > std::hypot(side.first, side.second)) {
            side = {across, down};
        }
    }
    return out;
}

gamepad::Sticks TouchLayer::sticks() const {
    const Leans now = leans();
    return gamepad::Sticks{.leftX = onStickScale(now[0].first),
                           .leftY = onStickScale(now[0].second),
                           .rightX = onStickScale(now[1].first),
                           .rightY = onStickScale(now[1].second)};
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
    const Leans now = leans();
    const auto sideOf = [&now](StickSide side) {
        return now.at(side == StickSide::Left ? 0 : 1);
    };
    for (const Piece& piece : plan_.pieces) {
        const bool pushed = piece.pushed && [&] {
            const auto [across, down] = sideOf(piece.pushed->side);
            return (across * piece.pushed->x) + (down * piece.pushed->y) > 0.5F;
        }();
        const bool down = pushed || std::ranges::any_of(piece.lit, [this](Button button) {
                              return pressed(button);
                          });
        const Art& shown = down && !piece.down.rgba.empty() ? piece.down : piece.art;
        auto [leanX, leanY] = leaning(piece.lean);
        if (piece.knob) {
            const auto [across, along] = sideOf(*piece.knob);
            leanX = across * piece.lean;
            leanY = along * piece.lean;
        }
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
