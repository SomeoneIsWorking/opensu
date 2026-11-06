#include "touch/pad_layout.hpp"

#include <array>

namespace opensu::touch {
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

constexpr std::array<Button, 4> faceButtons{Button::A, Button::B, Button::X, Button::Y};
constexpr std::array<const char*, 4> faceFiles{"face_a.svg", "face_b.svg", "face_x.svg",
                                               "face_y.svg"};

/// `box`'s circle reaching past what is drawn.
Box roundHit(const Box& box) {
    return box.grown(box.width * 0.5F * (reach - 1.0F));
}

/// A rectangular button: `box`, reaching `slack` past it.
Control rectangle(const Box& box, float slack, Button button) {
    return Control{.hit = box.grown(slack),
                   .round = false,
                   .stick = std::nullopt,
                   .targets = {Target{box.centreX(), box.centreY(), button}}};
}

} // namespace

ControlPlan padLayout(const WindowSurface& surface, const ArtSource& art) {
    const float dp = surface.dp;
    const float m = margin * dp;
    const float left = surface.safeX;
    const float top = surface.safeY;
    const float right = surface.safeX + surface.safeWidth;
    const float bottom = surface.safeY + surface.safeHeight;
    const float padSide = padSize * dp;
    const Box pad{left + m, bottom - m - padSide, padSide, padSide};
    const float faceSide = faceSize * dp;
    const float spread = faceSpread * dp;
    const float faceX = right - m - spread - (faceSide * 0.5F);
    const float faceY = pad.y + (padSide * 0.5F);
    const auto faceAt = [&](float x, float y) {
        return Box{x - (faceSide * 0.5F), y - (faceSide * 0.5F), faceSide, faceSide};
    };
    // Xbox positions: A bottom, B right, X left, Y top.
    const std::array<Box, 4> face{faceAt(faceX, faceY + spread), faceAt(faceX + spread, faceY),
                                  faceAt(faceX - spread, faceY), faceAt(faceX, faceY - spread)};
    const float cluster = (spread * 2.0F) + faceSide;
    const Box faceCluster{faceX - (cluster * 0.5F), faceY - (cluster * 0.5F), cluster, cluster};
    const Box l1{left + m, top + m, shoulderWidth * dp, shoulderHeight * dp};
    const Box r1{right - m - (shoulderWidth * dp), top + m, shoulderWidth * dp,
                 shoulderHeight * dp};
    const float middle = left + (surface.safeWidth * 0.5F);
    const float gap = 10.0F * dp;
    const float pillY = bottom - m - (pillHeight * dp);
    const Box select{middle - gap - (pillWidth * dp), pillY, pillWidth * dp, pillHeight * dp};
    const Box start{middle + gap, pillY, pillWidth * dp, pillHeight * dp};
    const Box pause{middle - (pauseSize * dp * 0.5F), top + m, pauseSize * dp, pauseSize * dp};

    ControlPlan plan;
    plan.pieces.push_back({pad,
                           art.svg("dpad.svg", pad),
                           {Button::Up, Button::Down, Button::Left, Button::Right}});
    for (std::size_t at = 0; at < face.size(); ++at) {
        plan.pieces.push_back(
            {face.at(at), art.svg(faceFiles.at(at), face.at(at)), {faceButtons.at(at)}});
    }
    plan.pieces.push_back({l1, art.svg("shoulder_l.svg", l1), {Button::L1}});
    plan.pieces.push_back({r1, art.svg("shoulder_r.svg", r1), {Button::R1}});
    plan.pieces.push_back({select, art.svg("select.svg", select), {Button::Select}});
    plan.pieces.push_back({start, art.svg("start.svg", start), {Button::Start}});
    plan.pieces.push_back({pause, art.svg("pause.svg", pause), {Button::Guide}});

    const float slack = (reach - 1.0F) * faceSize * 0.5F * dp;
    plan.controls.push_back(Control{.hit = roundHit(pause),
                                    .round = true,
                                    .stick = std::nullopt,
                                    .targets = {Target{pause.centreX(), pause.centreY(),
                                                       Button::Guide}}});
    plan.controls.push_back(
        Control{.hit = roundHit(pad),
                .round = true,
                .stick = Stick{pad.centreX(), pad.centreY(), deadZone * dp},
                .targets = {}});
    Control faces{.hit = roundHit(faceCluster), .round = true, .stick = std::nullopt, .targets = {}};
    for (std::size_t at = 0; at < face.size(); ++at) {
        faces.targets.push_back(
            Target{face.at(at).centreX(), face.at(at).centreY(), faceButtons.at(at)});
    }
    plan.controls.push_back(std::move(faces));
    plan.controls.push_back(rectangle(l1, slack, Button::L1));
    plan.controls.push_back(rectangle(r1, slack, Button::R1));
    plan.controls.push_back(rectangle(select, slack, Button::Select));
    plan.controls.push_back(rectangle(start, slack, Button::Start));
    return plan;
}

} // namespace opensu::touch
