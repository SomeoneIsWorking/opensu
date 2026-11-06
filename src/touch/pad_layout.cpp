#include "touch/pad_layout.hpp"

#include <array>
#include <utility>

namespace opensu::touch {
namespace {

using gamepad::Button;

constexpr float margin = 24.0F;
constexpr float padSize = 168.0F;
constexpr float faceSize = 64.0F;
constexpr float faceSpread = 58.0F;
constexpr float shoulderWidth = 96.0F;
constexpr float shoulderHeight = 44.0F;
constexpr float pillWidth = 80.0F;
constexpr float pillHeight = 34.0F;
constexpr float pauseSize = 46.0F;
/// What must stay apart: the d-pad from the faces, the shoulders from both.
constexpr float clearance = 16.0F;
/// Hit regions reach this much past what is drawn, so a thumb need not land inside.
constexpr float reach = 1.4F;
/// Nearer the d-pad's centre than this, no direction is held.
constexpr float deadZone = 16.0F;

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

ControlPlan padLayout(const WindowSurface& surface, const ArtSource& art, bool rotate) {
    const bool upright = surface.upright();
    const float faceArea = (faceSpread * 2.0F) + faceSize;
    const float tall = (margin * 2.0F) + faceArea + shoulderHeight + clearance;
    const float dp =
        surface.dpWithin((margin * 2.0F) + padSize + clearance + faceArea,
                         upright ? (tall + pillHeight + clearance) / uprightShare : tall);
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
    // Upright the rows stand above the d-pad and the faces, under the picture.
    const float apart = clearance * dp;
    const float pillY =
        upright ? faceCluster.y - apart - (pillHeight * dp) : bottom - m - (pillHeight * dp);
    const float rowY = upright ? pillY - apart - (pauseSize * dp) : top + m;
    const Box l1{left + m, rowY, shoulderWidth * dp, shoulderHeight * dp};
    const Box r1{right - m - (shoulderWidth * dp), rowY, shoulderWidth * dp, shoulderHeight * dp};
    const float middle = left + (surface.safeWidth * 0.5F);
    const float gap = 10.0F * dp;
    const Box select{middle - gap - (pillWidth * dp), pillY, pillWidth * dp, pillHeight * dp};
    const Box start{middle + gap, pillY, pillWidth * dp, pillHeight * dp};
    const Box pause{middle - (pauseSize * dp * 0.5F), rowY, pauseSize * dp, pauseSize * dp};
    const Box turn{pause.x + pause.width + gap, rowY, pause.width, pause.height};

    ControlPlan plan;
    plan.band = upright ? static_cast<float>(surface.height) - rowY + apart : 0.0F;
    plan.pieces.push_back(Piece{.box = pad,
                                .art = art.svg("dpad.svg", pad),
                                .lit = {Button::Up, Button::Down, Button::Left, Button::Right}});
    for (std::size_t at = 0; at < face.size(); ++at) {
        plan.pieces.push_back(Piece{.box = face.at(at),
                                    .art = art.svg(faceFiles.at(at), face.at(at)),
                                    .lit = {faceButtons.at(at)}});
    }
    plan.pieces.push_back(
        Piece{.box = l1, .art = art.svg("shoulder_l.svg", l1), .lit = {Button::L1}});
    plan.pieces.push_back(
        Piece{.box = r1, .art = art.svg("shoulder_r.svg", r1), .lit = {Button::R1}});
    plan.pieces.push_back(
        Piece{.box = select, .art = art.svg("select.svg", select), .lit = {Button::Select}});
    plan.pieces.push_back(
        Piece{.box = start, .art = art.svg("start.svg", start), .lit = {Button::Start}});
    plan.pieces.push_back(
        Piece{.box = pause, .art = art.svg("pause.svg", pause), .lit = {Button::Guide}});
    if (rotate) {
        plan.pieces.push_back(
            Piece{.box = turn, .art = art.svg("rotate.svg", turn), .lit = {Button::Rotate}});
    }

    const float slack = (reach - 1.0F) * faceSize * 0.5F * dp;
    plan.controls.push_back(
        Control{.hit = roundHit(pause),
                .round = true,
                .stick = std::nullopt,
                .targets = {Target{pause.centreX(), pause.centreY(), Button::Guide}}});
    if (rotate) {
        plan.controls.push_back(
            Control{.hit = roundHit(turn),
                    .round = true,
                    .stick = std::nullopt,
                    .targets = {Target{turn.centreX(), turn.centreY(), Button::Rotate}}});
    }
    plan.controls.push_back(
        Control{.hit = roundHit(pad),
                .round = true,
                .stick = Stick{.x = pad.centreX(), .y = pad.centreY(), .deadZone = deadZone * dp},
                .targets = {}});
    Control faces{
        .hit = roundHit(faceCluster), .round = true, .stick = std::nullopt, .targets = {}};
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
