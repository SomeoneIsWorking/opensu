#include "touch_layer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "svg/svg_raster.hpp"

namespace opensu::retro {
namespace {

using gamepad::Button;

constexpr float margin = 28.0F;
constexpr float padSize = 140.0F;
constexpr float faceSize = 52.0F;
constexpr float faceSpread = 47.0F;
constexpr float shoulderWidth = 84.0F;
constexpr float shoulderHeight = 38.0F;
constexpr float pillWidth = 70.0F;
constexpr float pillHeight = 30.0F;
constexpr float pauseSize = 42.0F;
/// Hit regions reach this much past what is drawn, so a thumb need not land inside.
constexpr float reach = 1.4F;
/// Nearer the d-pad's centre than this, no direction is held.
constexpr float deadZone = 14.0F;
constexpr float idleOpacity = 0.65F;

constexpr std::array<Button, 4> faceButtons{Button::A, Button::B, Button::X, Button::Y};
constexpr std::array<const char*, 4> faceFiles{"face_a.svg", "face_b.svg", "face_x.svg",
                                               "face_y.svg"};

float centreX(float x, float width) {
    return x + (width * 0.5F);
}

} // namespace

TouchLayer::TouchLayer(std::filesystem::path art) : artDir_{std::move(art)} {
}

TouchLayer::Art TouchLayer::rasterised(const char* file, const Box& box) const {
    const int width = std::max(1, static_cast<int>(std::lround(box.width)));
    const int height = std::max(1, static_cast<int>(std::lround(box.height)));
    std::optional<std::vector<unsigned char>> rgba = svg::rasterise(artDir_ / file, width, height);
    if (!rgba) {
        return {};
    }
    return Art{std::move(*rgba), width, height};
}

void TouchLayer::layout(const WindowSurface& surface) {
    touches_.clear();
    dp_ = surface.dp;
    width_ = surface.width;
    height_ = surface.height;
    const float dp = surface.dp;
    const float m = margin * dp;
    const float left = surface.safeX;
    const float top = surface.safeY;
    const float right = surface.safeX + surface.safeWidth;
    const float bottom = surface.safeY + surface.safeHeight;
    const float padSide = padSize * dp;
    pad_ = {left + m, bottom - m - padSide, padSide, padSide};
    const float faceSide = faceSize * dp;
    const float spread = faceSpread * dp;
    const float faceX = right - m - spread - (faceSide * 0.5F);
    const float faceY = pad_.y + (padSide * 0.5F);
    const auto faceAt = [&](float x, float y) {
        return Box{x - (faceSide * 0.5F), y - (faceSide * 0.5F), faceSide, faceSide};
    };
    // Xbox positions: A bottom, B right, X left, Y top.
    face_ = {faceAt(faceX, faceY + spread), faceAt(faceX + spread, faceY),
             faceAt(faceX - spread, faceY), faceAt(faceX, faceY - spread)};
    const float cluster = (spread * 2.0F) + faceSide;
    faceCluster_ = {faceX - (cluster * 0.5F), faceY - (cluster * 0.5F), cluster, cluster};
    l1_ = {left + m, top + m, shoulderWidth * dp, shoulderHeight * dp};
    r1_ = {right - m - (shoulderWidth * dp), top + m, shoulderWidth * dp, shoulderHeight * dp};
    const float middle = centreX(left, surface.safeWidth);
    const float gap = 10.0F * dp;
    const float pillY = bottom - m - (pillHeight * dp);
    select_ = {middle - gap - (pillWidth * dp), pillY, pillWidth * dp, pillHeight * dp};
    start_ = {middle + gap, pillY, pillWidth * dp, pillHeight * dp};
    pause_ = {middle - (pauseSize * dp * 0.5F), top + m, pauseSize * dp, pauseSize * dp};

    padArt_ = rasterised("dpad.svg", pad_);
    for (std::size_t at = 0; at < face_.size(); ++at) {
        faceArt_.at(at) = rasterised(faceFiles.at(at), face_.at(at));
    }
    l1Art_ = rasterised("shoulder_l.svg", l1_);
    r1Art_ = rasterised("shoulder_r.svg", r1_);
    selectArt_ = rasterised("select.svg", select_);
    startArt_ = rasterised("start.svg", start_);
    pauseArt_ = rasterised("pause.svg", pause_);
    dirty_ = true;
}

std::optional<TouchLayer::Control> TouchLayer::controlAt(float x, float y) const {
    const auto inRound = [&](const Box& box) {
        const float radius = box.width * 0.5F * reach;
        return std::hypot(x - centreX(box.x, box.width), y - (box.y + (box.height * 0.5F))) <=
               radius;
    };
    const float slack = (reach - 1.0F) * faceSize * 0.5F * dp_;
    if (inRound(pause_)) {
        return Control::Pause;
    }
    if (inRound(pad_)) {
        return Control::Pad;
    }
    if (inRound(faceCluster_)) {
        return Control::Face;
    }
    if (l1_.grown(slack).contains(x, y)) {
        return Control::L1;
    }
    if (r1_.grown(slack).contains(x, y)) {
        return Control::R1;
    }
    if (select_.grown(slack).contains(x, y)) {
        return Control::Select;
    }
    if (start_.grown(slack).contains(x, y)) {
        return Control::Start;
    }
    return std::nullopt;
}

std::vector<Button> TouchLayer::buttonsAt(Control control, float x, float y) const {
    switch (control) {
    case Control::Pad: {
        const float dx = x - centreX(pad_.x, pad_.width);
        const float dy = y - (pad_.y + (pad_.height * 0.5F));
        if (std::hypot(dx, dy) < deadZone * dp_) {
            return {};
        }
        // Eight sectors: a diagonal holds both of its directions.
        const auto sector =
            static_cast<int>(std::lround(std::atan2(dy, dx) / (std::numbers::pi_v<float> / 4.0F))) &
            7;
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
    case Control::Face: {
        std::size_t nearest = 0;
        float best = 0.0F;
        for (std::size_t at = 0; at < face_.size(); ++at) {
            const Box& box = face_.at(at);
            const float distance =
                std::hypot(x - centreX(box.x, box.width), y - (box.y + (box.height * 0.5F)));
            if (at == 0 || distance < best) {
                best = distance;
                nearest = at;
            }
        }
        return {faceButtons.at(nearest)};
    }
    case Control::L1:
        return {Button::L1};
    case Control::R1:
        return {Button::R1};
    case Control::Select:
        return {Button::Select};
    case Control::Start:
        return {Button::Start};
    case Control::Pause:
        return {Button::Guide};
    }
    return {};
}

bool TouchLayer::finger(const Finger& finger) {
    const std::vector<Button> before = held();
    switch (finger.phase) {
    case Finger::Phase::Down:
        if (const std::optional<Control> control = controlAt(finger.x, finger.y)) {
            touches_.insert_or_assign(finger.id,
                                      Touch{*control, buttonsAt(*control, finger.x, finger.y)});
        }
        break;
    case Finger::Phase::Move:
        if (auto touch = touches_.find(finger.id); touch != touches_.end()) {
            touch->second.buttons = buttonsAt(touch->second.control, finger.x, finger.y);
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
    const auto shown = [](bool down) {
        return down ? 1.0F : idleOpacity;
    };
    const bool steering = pressed(Button::Up) || pressed(Button::Down) || pressed(Button::Left) ||
                          pressed(Button::Right);
    compose(padArt_, pad_, shown(steering));
    for (std::size_t at = 0; at < face_.size(); ++at) {
        compose(faceArt_.at(at), face_.at(at), shown(pressed(faceButtons.at(at))));
    }
    compose(l1Art_, l1_, shown(pressed(Button::L1)));
    compose(r1Art_, r1_, shown(pressed(Button::R1)));
    compose(selectArt_, select_, shown(pressed(Button::Select)));
    compose(startArt_, start_, shown(pressed(Button::Start)));
    compose(pauseArt_, pause_, shown(pressed(Button::Guide)));
}

OverlayImage TouchLayer::image() {
    const bool changed = std::exchange(dirty_, false);
    if (changed) {
        draw();
    }
    return OverlayImage{pixels_, width_, height_, changed};
}

} // namespace opensu::retro
