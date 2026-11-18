#include "touch/console_layout.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "touch/layout_parts.hpp"

namespace opensu::touch {
namespace {

using gamepad::Button;
using settings::TouchLayout;

constexpr float margin = 24.0F;
constexpr float mainSize = 168.0F;
constexpr float auxSize = 120.0F;
constexpr float faceSize = 64.0F;
constexpr float faceSpread = 58.0F;
constexpr float pairFace = 72.0F;
constexpr float bigFace = 96.0F;
constexpr float smallFace = 44.0F;
constexpr float shoulderWidth = 96.0F;
constexpr float shoulderHeight = 44.0F;
constexpr float pillWidth = 80.0F;
constexpr float pillHeight = 34.0F;
constexpr float pauseSize = 46.0F;
constexpr float pedalWidth = 90.0F;
constexpr float pedalHeight = 170.0F;
constexpr float steerWidth = 230.0F;
constexpr float steerHeight = 84.0F;
/// What must stay apart: the clusters from each other, the rows from the clusters.
constexpr float clearance = 16.0F;
constexpr float gap = 10.0F;
/// Hit regions reach this much past what is drawn, so a thumb need not land inside.
constexpr float reach = 1.4F;
/// Nearer a d-pad's centre than this, no direction is held.
constexpr float deadZone = 16.0F;
/// Of a stick's ring: its thumb, how far the thumb moves, how far a finger leans it all the way
/// and how near the centre leans it not at all.
constexpr float thumbShare = 0.5F;
constexpr float thumbTravel = 0.3F;
constexpr float stickReach = 0.42F;
constexpr float stickDead = 0.1F;
/// A word on a button, as a share of its height, and the most of its width it takes.
constexpr float wordHeight = 0.42F;
constexpr float wordWidth = 0.8F;
/// How far up each button of a row sits, as a share of a button: the arc a thumb makes.
constexpr std::array<float, 3> rowRise{0.0F, 0.18F, 0.3F};
/// Of a pair: how far each button sits from its centre across and down, as a share of a button.
constexpr float pairAcross = 0.58F;
constexpr float pairDown = 0.38F;

enum class Cluster : std::uint8_t {
    None,
    Dpad,
    LeftStick,
    RightStick,
    /// Steering: analog, or Left and Right.
    Steer,
    SteerDigital,
    /// Four faces: bottom, right, left, top.
    Diamond,
    /// Two faces, the first lower left, the second upper right.
    Rising,
    /// Two faces, the first lower right, the second upper left.
    Falling,
    /// Three faces left to right, rising.
    Row,
    /// Two such rows, the first three lower.
    TwoRows,
    /// Nintendo 64's C buttons: the right stick pushed all the way.
    CButtons,
    /// Two pedals, the first on the left.
    Pedals,
    /// A stick holding its directions, as a d-pad does.
    Move,
    /// One big button.
    Fire,
};

/// A button: what it holds, and its drawing or else the word on a blank one.
struct Face {
    Button button{Button::A};
    const char* file{nullptr};
    const char* word{""};
};

struct Description {
    Cluster leftMain{Cluster::Dpad};
    Cluster leftAux{Cluster::None};
    Cluster rightMain{Cluster::Diamond};
    Cluster rightAux{Cluster::None};
    /// The right main cluster's buttons, in its cluster's order.
    std::vector<Face> faces;
    float faceSide{faceSize};
    /// Outer first.
    std::vector<Face> leftShoulders;
    std::vector<Face> rightShoulders;
    /// Left to right.
    std::vector<Face> pills;
    /// The Fire cluster's button.
    Face fire{};
    /// The rest of the right half turns the view.
    bool look{false};
};

std::vector<Face> selectStart() {
    return {{Button::Select, "select.svg"}, {Button::Start, "start.svg"}};
}

std::vector<Face> shoulderLetters() {
    return {{Button::L1, "shoulder_l.svg"}};
}

bool readsSticks(const LayoutWish& wish) {
    return wish.analog || wish.console == TouchLayout::Nintendo64 ||
           wish.console == TouchLayout::Gamepad;
}

Description describe(const LayoutWish& wish);

/// What a genre's layout keeps of its console's: the shoulders and pills its own buttons do not
/// already hold.
void keepConsoles(Description& genre, const LayoutWish& wish) {
    const bool consoleOwn =
        wish.console != TouchLayout::Racing && wish.console != TouchLayout::Platformer;
    const Description base =
        describe(LayoutWish{.layout = consoleOwn ? wish.console : TouchLayout::Pad,
                            .console = wish.console,
                            .analog = wish.analog});
    const auto free = [&genre](const Face& face) {
        return std::ranges::none_of(genre.faces, [&face](const Face& own) {
            return own.button == face.button;
        });
    };
    std::ranges::copy_if(base.leftShoulders, std::back_inserter(genre.leftShoulders), free);
    std::ranges::copy_if(base.rightShoulders, std::back_inserter(genre.rightShoulders), free);
    std::ranges::copy_if(base.pills, std::back_inserter(genre.pills), free);
}

Description describe(const LayoutWish& wish) {
    Description d;
    d.pills = selectStart();
    switch (wish.layout) {
    case TouchLayout::TwoButtons:
    case TouchLayout::GameBoyAdvance:
        d.rightMain = Cluster::Rising;
        d.faces = {{Button::A, nullptr, "B"}, {Button::B, nullptr, "A"}};
        d.faceSide = pairFace;
        if (wish.layout == TouchLayout::GameBoyAdvance) {
            d.leftShoulders = {{Button::L1, "shoulder_l.svg"}};
            d.rightShoulders = {{Button::R1, "shoulder_r.svg"}};
        }
        return d;
    case TouchLayout::SuperNintendo:
        d.faces = {{Button::A, nullptr, "B"},
                   {Button::B, nullptr, "A"},
                   {Button::X, nullptr, "Y"},
                   {Button::Y, nullptr, "X"}};
        d.leftShoulders = {{Button::L1, "shoulder_l.svg"}};
        d.rightShoulders = {{Button::R1, "shoulder_r.svg"}};
        return d;
    case TouchLayout::Nintendo64:
        d.leftMain = Cluster::LeftStick;
        d.leftAux = Cluster::Dpad;
        d.rightMain = Cluster::Falling;
        d.rightAux = Cluster::CButtons;
        d.faces = {{Button::A, nullptr, "A"}, {Button::X, nullptr, "B"}};
        d.faceSide = pairFace;
        d.leftShoulders = {{Button::L1, nullptr, "L"}, {Button::L2, nullptr, "Z"}};
        d.rightShoulders = {{Button::R1, nullptr, "R"}};
        d.pills = {{Button::Start, "start.svg"}};
        return d;
    case TouchLayout::PlayStation:
        d.faces = {{Button::A, "face_cross.svg"},
                   {Button::B, "face_circle.svg"},
                   {Button::X, "face_square.svg"},
                   {Button::Y, "face_triangle.svg"}};
        d.leftShoulders = {{Button::L1, nullptr, "L1"}, {Button::L2, nullptr, "L2"}};
        d.rightShoulders = {{Button::R1, nullptr, "R1"}, {Button::R2, nullptr, "R2"}};
        if (wish.analog) {
            d.leftAux = Cluster::LeftStick;
            d.rightAux = Cluster::RightStick;
            d.pills = {{Button::Select, "select.svg"},
                       {Button::L3, nullptr, "L3"},
                       {Button::R3, nullptr, "R3"},
                       {Button::Start, "start.svg"}};
        }
        return d;
    case TouchLayout::Genesis3:
    case TouchLayout::Genesis6:
        d.rightMain = Cluster::Row;
        d.faces = {{Button::X, nullptr, "A"}, {Button::A, nullptr, "B"}, {Button::B, nullptr, "C"}};
        d.pills = {{Button::Start, "start.svg"}};
        if (wish.layout == TouchLayout::Genesis6) {
            d.rightMain = Cluster::TwoRows;
            d.faces.insert(d.faces.end(), {{Button::L1, nullptr, "X"},
                                           {Button::Y, nullptr, "Y"},
                                           {Button::R1, nullptr, "Z"}});
            d.pills = {{Button::Select, nullptr, "Mode"}, {Button::Start, "start.svg"}};
        }
        return d;
    case TouchLayout::Gamepad:
        d.leftMain = Cluster::LeftStick;
        d.leftAux = Cluster::Dpad;
        d.rightAux = Cluster::RightStick;
        d.faces = {{Button::A, "face_a.svg"},
                   {Button::B, "face_b.svg"},
                   {Button::X, "face_x.svg"},
                   {Button::Y, "face_y.svg"}};
        d.leftShoulders = {{Button::L1, nullptr, "LB"}, {Button::L2, nullptr, "LT"}};
        d.rightShoulders = {{Button::R1, nullptr, "RB"}, {Button::R2, nullptr, "RT"}};
        d.pills = {{Button::Select, nullptr, "Back"}, {Button::Start, "start.svg"}};
        return d;
    case TouchLayout::Racing: {
        const bool triggers = wish.console == TouchLayout::Gamepad;
        d.leftMain = readsSticks(wish) ? Cluster::Steer : Cluster::SteerDigital;
        d.rightMain = Cluster::Pedals;
        d.faces = {{triggers ? Button::L2 : Button::X, nullptr, "Brake"},
                   {triggers ? Button::R2 : Button::A, nullptr, "Gas"}};
        d.pills.clear();
        keepConsoles(d, wish);
        return d;
    }
    case TouchLayout::Platformer:
        d.leftMain = readsSticks(wish) ? Cluster::LeftStick : Cluster::Dpad;
        d.rightMain = Cluster::Falling;
        d.faces = {{Button::A, nullptr, "Jump"}, {Button::X, nullptr, "Action"}};
        d.faceSide = bigFace;
        d.pills.clear();
        keepConsoles(d, wish);
        return d;
    case TouchLayout::Shooter:
        if (!wish.keyboard) {
            return describe(LayoutWish{.layout = wish.console,
                                       .console = wish.console,
                                       .analog = wish.analog,
                                       .keyboard = false});
        }
        d.leftMain = Cluster::Move;
        d.rightAux = Cluster::Fire;
        d.faces = {{Button::A, nullptr, "Jump"},
                   {Button::B, nullptr, "Crouch"},
                   {Button::X, nullptr, "Reload"},
                   {Button::Y, nullptr, "Use"}};
        d.fire = {Button::R2, nullptr, "Fire"};
        d.leftShoulders = {{Button::L2, nullptr, "Aim"}, {Button::L3, nullptr, "Sprint"}};
        d.rightShoulders = {{Button::R1, nullptr, "G"}, {Button::L1, nullptr, "Q"}};
        d.pills = {{Button::Select, nullptr, "Tab"}, {Button::Start, nullptr, "Esc"}};
        d.look = true;
        return d;
    case TouchLayout::Auto:
    case TouchLayout::Pad:
        break;
    }
    d.faces = {{Button::A, "face_a.svg"},
               {Button::B, "face_b.svg"},
               {Button::X, "face_x.svg"},
               {Button::Y, "face_y.svg"}};
    d.leftShoulders = shoulderLetters();
    d.rightShoulders = {{Button::R1, "shoulder_r.svg"}};
    return d;
}

struct Size {
    float width{0.0F};
    float height{0.0F};
};

/// A cluster's size in dp, as the main one of its side or the smaller one beside it.
Size sizeOf(Cluster cluster, bool aux, float side) {
    const float square = aux ? auxSize : mainSize;
    switch (cluster) {
    case Cluster::None:
        return {};
    case Cluster::Dpad:
    case Cluster::LeftStick:
    case Cluster::RightStick:
    case Cluster::Move:
        return {square, square};
    case Cluster::Fire:
        return {bigFace, bigFace};
    case Cluster::Steer:
    case Cluster::SteerDigital:
        return {steerWidth, steerHeight};
    case Cluster::Diamond:
        return {(faceSpread * 2.0F) + faceSize, (faceSpread * 2.0F) + faceSize};
    case Cluster::Rising:
    case Cluster::Falling:
        return {side * (1.0F + (pairAcross * 2.0F)), side * (1.0F + (pairDown * 2.0F))};
    case Cluster::Row:
        return {(side * 3.0F) + (gap * 2.0F), side * (1.0F + rowRise.back())};
    case Cluster::TwoRows:
        return {(side * 3.0F) + (gap * 2.0F), (side * (2.0F + rowRise.back())) + gap};
    case Cluster::CButtons:
        return {auxSize, auxSize};
    case Cluster::Pedals:
        return {(pedalWidth * 2.0F) + gap, pedalHeight};
    }
    return {};
}

float rowWidth(const std::vector<Face>& faces, float width) {
    const auto count = static_cast<float>(faces.size());
    return count == 0.0F ? 0.0F : (count * width) + ((count - 1.0F) * gap);
}

/// Lays out a description on one surface at one scale.
class Builder {
  public:
    Builder(const ArtSource& art, LabelFace& labels, float dp)
        : art_{art}, labels_{labels}, dp_{dp} {
    }

    ControlPlan take() {
        return std::move(plan_);
    }

    /// A button drawn in `box`: its drawing, or a blank one with its word.
    void drawFace(const Box& box, const Face& face, const char* blank) {
        if (face.file != nullptr) {
            plan_.pieces.push_back(
                Piece{.box = box, .art = art_.svg(face.file, box), .lit = {face.button}});
            return;
        }
        plan_.pieces.push_back(
            Piece{.box = box, .art = art_.svg(blank, box), .lit = {face.button}});
        const float idle = Piece{}.idle;
        plan_.pieces.push_back(
            labelIn(box, labels_.label(face.word, box.height * wordHeight, box.width * wordWidth),
                    {face.button}, idle));
    }

    /// A button of its own: a finger takes it within `slack` of it.
    void button(const Box& box, const Face& face, const char* blank, bool round) {
        drawFace(box, face, blank);
        const float slack =
            round ? box.width * 0.5F * (reach - 1.0F) : (reach - 1.0F) * faceSize * 0.5F * dp_;
        plan_.controls.push_back(
            Control{.hit = box.grown(slack),
                    .round = round,
                    .stick = std::nullopt,
                    .targets = {Target{box.centreX(), box.centreY(), face.button}}});
    }

    void row(const std::vector<Face>& faces, float x, float y, float width, float height,
             bool rightwards) {
        for (std::size_t at = 0; at < faces.size(); ++at) {
            const float step = static_cast<float>(at) * (width + (gap * dp_));
            const Box box{rightwards ? x + step : x - step - width, y, width, height};
            button(box, faces[at], "pill.svg", false);
        }
    }

    /// A finger in `area` that lands on no control turns the view.
    void look(const Box& area) {
        plan_.controls.push_back(Control{
            .hit = area, .round = false, .stick = std::nullopt, .targets = {}, .look = true});
    }

    void cluster(Cluster kind, const Box& box, const Description& d) {
        switch (kind) {
        case Cluster::None:
            return;
        case Cluster::Dpad:
            dpad(box);
            return;
        case Cluster::LeftStick:
            stick(box, StickSide::Left);
            return;
        case Cluster::Move:
            move(box);
            return;
        case Cluster::Fire:
            button(box, d.fire, "button.svg", true);
            return;
        case Cluster::RightStick:
            stick(box, StickSide::Right);
            return;
        case Cluster::Steer:
        case Cluster::SteerDigital:
            steer(box, kind == Cluster::Steer);
            return;
        case Cluster::CButtons:
            cButtons(box);
            return;
        case Cluster::Pedals:
            pedals(box, d.faces);
            return;
        case Cluster::Diamond:
        case Cluster::Rising:
        case Cluster::Falling:
        case Cluster::Row:
        case Cluster::TwoRows:
            faces(kind, box, d);
            return;
        }
    }

  private:
    void dpad(const Box& box) {
        plan_.pieces.push_back(
            Piece{.box = box,
                  .art = art_.svg("dpad.svg", box),
                  .lit = {Button::Up, Button::Down, Button::Left, Button::Right}});
        plan_.controls.push_back(Control{
            .hit = box.grown(box.width * 0.5F * (reach - 1.0F)),
            .round = true,
            .stick = Stick{.x = box.centreX(), .y = box.centreY(), .deadZone = deadZone * dp_},
            .targets = {}});
    }

    void stick(const Box& ring, StickSide side) {
        plan_.pieces.push_back(Piece{.box = ring, .art = art_.svg("stick_ring.svg", ring)});
        const float thumb = ring.width * thumbShare;
        const Box knob{ring.centreX() - (thumb * 0.5F), ring.centreY() - (thumb * 0.5F), thumb,
                       thumb};
        plan_.pieces.push_back(Piece{.box = knob,
                                     .art = art_.svg("stick_thumb.svg", knob),
                                     .lean = ring.width * thumbTravel,
                                     .knob = side});
        plan_.controls.push_back(Control{.hit = ring.grown(ring.width * 0.5F * (reach - 1.0F)),
                                         .round = true,
                                         .stick = Stick{.x = ring.centreX(),
                                                        .y = ring.centreY(),
                                                        .deadZone = ring.width * stickDead,
                                                        .ways = 0,
                                                        .analog = side,
                                                        .reach = ring.width * stickReach},
                                         .targets = {}});
    }

    void move(const Box& ring) {
        const std::vector<Button> ways{Button::Up, Button::Down, Button::Left, Button::Right};
        plan_.pieces.push_back(Piece{.box = ring, .art = art_.svg("stick_ring.svg", ring)});
        const float thumb = ring.width * thumbShare;
        const Box knob{ring.centreX() - (thumb * 0.5F), ring.centreY() - (thumb * 0.5F), thumb,
                       thumb};
        plan_.pieces.push_back(Piece{.box = knob,
                                     .art = art_.svg("stick_thumb.svg", knob),
                                     .lit = ways,
                                     .lean = ring.width * thumbTravel});
        plan_.controls.push_back(Control{
            .hit = ring.grown(ring.width * 0.5F * (reach - 1.0F)),
            .round = true,
            .stick =
                Stick{.x = ring.centreX(), .y = ring.centreY(), .deadZone = ring.width * stickDead},
            .targets = {}});
    }

    void steer(const Box& track, bool analog) {
        plan_.pieces.push_back(Piece{.box = track,
                                     .art = art_.svg("pill.svg", track),
                                     .lit = {Button::Left, Button::Right}});
        const float side = track.height;
        const Box knob{track.centreX() - (side * 0.5F), track.y, side, side};
        const float travel = (track.width - side) * 0.5F;
        plan_.pieces.push_back(
            Piece{.box = knob,
                  .art = art_.svg("stick_thumb.svg", knob),
                  .lean = travel,
                  .knob = analog ? std::optional{StickSide::Left} : std::nullopt});
        Stick wheel{.x = track.centreX(),
                    .y = track.centreY(),
                    .deadZone = track.height * stickDead,
                    .ways = 2,
                    .vertical = false};
        if (analog) {
            wheel.analog = StickSide::Left;
            wheel.reach = travel;
        }
        plan_.controls.push_back(Control{.hit = track.grown(track.height * (reach - 1.0F)),
                                         .round = false,
                                         .stick = wheel,
                                         .targets = {}});
    }

    void cButtons(const Box& box) {
        const float side = smallFace * dp_;
        const float spread = (box.width - side) * 0.5F;
        const std::array<StickPush, 4> pushes{{{StickSide::Right, 0.0F, 1.0F},
                                               {StickSide::Right, 1.0F, 0.0F},
                                               {StickSide::Right, -1.0F, 0.0F},
                                               {StickSide::Right, 0.0F, -1.0F}}};
        for (const StickPush& push : pushes) {
            const Box face{box.centreX() + (push.x * spread) - (side * 0.5F),
                           box.centreY() + (push.y * spread) - (side * 0.5F), side, side};
            plan_.pieces.push_back(
                Piece{.box = face, .art = art_.svg("button.svg", face), .pushed = push});
            Piece word = labelIn(face, labels_.label("C", side * wordHeight, side * wordWidth), {},
                                 Piece{}.idle);
            word.pushed = push;
            plan_.pieces.push_back(std::move(word));
        }
        plan_.controls.push_back(Control{.hit = box.grown(side * 0.5F * (reach - 1.0F)),
                                         .round = true,
                                         .stick = Stick{.x = box.centreX(),
                                                        .y = box.centreY(),
                                                        .deadZone = side * 0.5F,
                                                        .ways = 4,
                                                        .analog = StickSide::Right,
                                                        .reach = spread},
                                         .targets = {}});
    }

    void pedals(const Box& box, const std::vector<Face>& faces) {
        const float width = (box.width - (gap * dp_)) * 0.5F;
        for (std::size_t at = 0; at < faces.size() && at < 2; ++at) {
            const Box pedal{box.x + (static_cast<float>(at) * (width + (gap * dp_))), box.y, width,
                            box.height};
            drawFace(pedal, faces[at], "pedal.svg");
            plan_.controls.push_back(
                Control{.hit = pedal.grown(gap * dp_),
                        .round = false,
                        .stick = std::nullopt,
                        .targets = {Target{pedal.centreX(), pedal.centreY(), faces[at].button}}});
        }
    }

    void faces(Cluster kind, const Box& box, const Description& d) {
        const float side = (kind == Cluster::Diamond ? faceSize : d.faceSide) * dp_;
        std::vector<std::pair<float, float>> centres;
        const float cx = box.centreX();
        const float cy = box.centreY();
        switch (kind) {
        case Cluster::Diamond: {
            const float spread = faceSpread * dp_;
            centres = {{cx, cy + spread}, {cx + spread, cy}, {cx - spread, cy}, {cx, cy - spread}};
            break;
        }
        case Cluster::Rising:
            centres = {{cx - (pairAcross * side), cy + (pairDown * side)},
                       {cx + (pairAcross * side), cy - (pairDown * side)}};
            break;
        case Cluster::Falling:
            centres = {{cx + (pairAcross * side), cy + (pairDown * side)},
                       {cx - (pairAcross * side), cy - (pairDown * side)}};
            break;
        case Cluster::Row:
        case Cluster::TwoRows: {
            const float lowest = box.y + box.height - (side * 0.5F);
            for (std::size_t at = 0; at < d.faces.size(); ++at) {
                const std::size_t column = at % 3;
                const float rows = at < 3 ? 0.0F : 1.0F;
                centres.emplace_back(
                    box.x + (side * 0.5F) + (static_cast<float>(column) * (side + (gap * dp_))),
                    lowest - (rowRise.at(column) * side) - (rows * (side + (gap * dp_))));
            }
            break;
        }
        default:
            return;
        }
        Control control{.hit = box.grown(side * 0.5F * (reach - 1.0F)),
                        .round = kind == Cluster::Diamond,
                        .stick = std::nullopt,
                        .targets = {}};
        for (std::size_t at = 0; at < d.faces.size() && at < centres.size(); ++at) {
            const auto [x, y] = centres[at];
            const Box face{x - (side * 0.5F), y - (side * 0.5F), side, side};
            drawFace(face, d.faces[at], "button.svg");
            control.targets.push_back(Target{x, y, d.faces[at].button});
        }
        plan_.controls.push_back(std::move(control));
    }

    const ArtSource& art_;
    LabelFace& labels_;
    float dp_;
    ControlPlan plan_;
};

} // namespace

ControlPlan consoleLayout(const WindowSurface& surface, const LayoutWish& wish,
                          const ArtSource& art, LabelFace& labels, bool rotate) {
    const Description d = describe(wish);
    const bool upright = surface.upright();
    const Size leftMain = sizeOf(d.leftMain, false, d.faceSide);
    const Size leftAux = sizeOf(d.leftAux, true, d.faceSide);
    const Size rightMain = sizeOf(d.rightMain, false, d.faceSide);
    const Size rightAux = sizeOf(d.rightAux, true, d.faceSide);
    const auto beside = [](Size main, Size aux) {
        return aux.width == 0.0F ? main.width : main.width + clearance + aux.width;
    };
    const auto stacked = [](Size main, Size aux) {
        return aux.height == 0.0F ? main.height : main.height + clearance + aux.height;
    };
    const float pills = rowWidth(d.pills, pillWidth);
    const float topRow = rowWidth(d.leftShoulders, shoulderWidth) +
                         rowWidth(d.rightShoulders, shoulderWidth) +
                         ((pauseSize + clearance) * (rotate ? 2.0F : 1.0F)) + (clearance * 2.0F);
    float wide = 0.0F;
    float tall = 0.0F;
    if (upright) {
        wide = std::max(leftMain.width, leftAux.width) + clearance +
               std::max(rightMain.width, rightAux.width);
        tall = std::max(stacked(leftMain, leftAux), stacked(rightMain, rightAux)) + clearance +
               pillHeight + clearance + std::max(pauseSize, shoulderHeight);
    } else {
        wide =
            beside(leftMain, leftAux) + clearance + pills + clearance + beside(rightMain, rightAux);
        tall = std::max({leftMain.height, leftAux.height, rightMain.height, rightAux.height}) +
               clearance + std::max(pauseSize, shoulderHeight);
    }
    wide = std::max({wide, topRow, pills}) + (margin * 2.0F);
    tall += margin * 2.0F;
    const float dp = surface.dpWithin(wide, upright ? tall / uprightShare : tall);
    const float m = margin * dp;
    const float apart = clearance * dp;
    const float left = surface.safeX;
    const float top = surface.safeY;
    const float right = surface.safeX + surface.safeWidth;
    const float bottom = surface.safeY + surface.safeHeight;
    const auto scaled = [dp](Size size) {
        return Size{size.width * dp, size.height * dp};
    };

    const Size lm = scaled(leftMain);
    const Size la = scaled(leftAux);
    const Size rm = scaled(rightMain);
    const Size ra = scaled(rightAux);
    const Box leftBox{left + m, bottom - m - lm.height, lm.width, lm.height};
    const Box rightBox{right - m - rm.width, bottom - m - rm.height, rm.width, rm.height};
    Box leftAuxBox{};
    Box rightAuxBox{};
    if (upright) {
        leftAuxBox = {std::max(left + m, leftBox.centreX() - (la.width * 0.5F)),
                      leftBox.y - apart - la.height, la.width, la.height};
        rightAuxBox = {std::min(right - m - ra.width, rightBox.centreX() - (ra.width * 0.5F)),
                       rightBox.y - apart - ra.height, ra.width, ra.height};
    } else {
        leftAuxBox = {leftBox.x + leftBox.width + apart, bottom - m - la.height, la.width,
                      la.height};
        rightAuxBox = {rightBox.x - apart - ra.width, bottom - m - ra.height, ra.width, ra.height};
    }
    float clustersTop = std::min(leftBox.y, rightBox.y);
    if (la.height > 0.0F) {
        clustersTop = std::min(clustersTop, leftAuxBox.y);
    }
    if (ra.height > 0.0F) {
        clustersTop = std::min(clustersTop, rightAuxBox.y);
    }
    const float pillY =
        upright ? clustersTop - apart - (pillHeight * dp) : bottom - m - (pillHeight * dp);
    const float rowHeight = std::max(pauseSize, shoulderHeight) * dp;
    const float rowY = upright ? pillY - apart - rowHeight : top + m;
    const float middle = left + (surface.safeWidth * 0.5F);

    Builder build{art, labels, dp};
    const Face pauseFace{Button::Guide, "pause.svg"};
    const Box pause{middle - (pauseSize * dp * 0.5F), rowY, pauseSize * dp, pauseSize * dp};
    build.button(pause, pauseFace, "button.svg", true);
    if (rotate) {
        const Box turn{pause.x + pause.width + (gap * dp), rowY, pause.width, pause.height};
        build.button(turn, Face{Button::Rotate, "rotate.svg"}, "button.svg", true);
    }
    build.cluster(d.leftMain, leftBox, d);
    build.cluster(d.leftAux, leftAuxBox, d);
    build.cluster(d.rightMain, rightBox, d);
    build.cluster(d.rightAux, rightAuxBox, d);
    const float shoulderY = rowY + ((rowHeight - (shoulderHeight * dp)) * 0.5F);
    build.row(d.leftShoulders, left + m, shoulderY, shoulderWidth * dp, shoulderHeight * dp, true);
    build.row(d.rightShoulders, right - m, shoulderY, shoulderWidth * dp, shoulderHeight * dp,
              false);
    build.row(d.pills, middle - (rowWidth(d.pills, pillWidth) * dp * 0.5F), pillY, pillWidth * dp,
              pillHeight * dp, true);

    if (d.look) {
        // Last, so every button on that side keeps its finger.
        build.look(Box{middle, top, right - middle, bottom - top});
    }
    ControlPlan plan = build.take();
    plan.band = upright ? static_cast<float>(surface.height) - rowY + apart : 0.0F;
    return plan;
}

} // namespace opensu::touch
