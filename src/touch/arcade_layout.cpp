#include "touch/arcade_layout.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <utility>

#include "gpu2d/geometry.hpp"
#include "svg/svg_raster.hpp"

namespace opensu::touch {
namespace {

using gamepad::Button;

constexpr float inset = 8.0F;
/// A thumb's size comes before staying on the black.
constexpr float stickSmallest = 168.0F;
constexpr float stickLargest = 180.0F;
constexpr float stickDeadZone = 0.12F;
constexpr float ballShare = 0.5F;
constexpr float leanShare = 0.2F;
/// The least a row shrinks to to stay on the black.
constexpr float buttonSmallest = 52.0F;
/// 13 mm, taken by a row that cannot stay on the black anyway.
constexpr float buttonLargest = 84.0F;
constexpr float buttonGap = 8.0F;
constexpr float buttonSlack = 0.2F;
/// How far above the panel's first column each column sits, as a share of a button: the arc a
/// hand makes.
constexpr std::array<float, 4> columnRise{0.0F, 0.26F, 0.34F, 0.26F};
constexpr float pillWidth = 76.0F;
constexpr float pillHeight = 36.0F;
constexpr float pillGap = 8.0F;
constexpr float pillSlack = 6.0F;
/// Between the pills and the buttons under them.
constexpr float pillDrop = 14.0F;
constexpr float pauseSize = 46.0F;
/// Of a button: the limb's drawing, and how far above centre it sits over its strength.
constexpr float limbShare = 0.56F;
constexpr float limbLift = 0.1F;
constexpr float strengthShare = 0.26F;
constexpr float strengthDrop = 0.3F;
constexpr float bottomMargin = 26.0F;
/// On black nothing is hidden, so the controls need not let anything through.
constexpr float idleOnBlack = 0.95F;
constexpr float idleOverPicture = 0.65F;

/// The cabinet's rows for a button count, top row first: Neo Geo's four in one row, Capcom's six
/// as two rows of three.
constexpr std::array<std::array<std::size_t, 2>, 9> rowsByCount{
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {3, 2}, {3, 3}, {4, 3}, {4, 4}}};

constexpr gpu2d::Colour artKey{255, 0, 0, 255};
constexpr std::array<gpu2d::Colour, 8> buttonColours{{{188, 44, 44, 255},
                                                      {190, 128, 16, 255},
                                                      {38, 134, 78, 255},
                                                      {40, 96, 186, 255},
                                                      {122, 66, 176, 255},
                                                      {16, 132, 136, 255},
                                                      {176, 66, 120, 255},
                                                      {96, 104, 118, 255}}};
constexpr gpu2d::Colour coinColour{176, 120, 16, 255};
constexpr gpu2d::Colour startColour{64, 72, 92, 255};

bool overlaps(const Box& one, const Box& other) {
    return one.x < other.x + other.width && other.x < one.x + one.width &&
           one.y < other.y + other.height && other.y < one.y + one.height;
}

/// Where a thing `width` wide goes: centred in the bar from `barStart` when it fits there, else
/// against the screen's edge, which `fromRight` says is the bar's far side.
float placed(float width, float barStart, float barWidth, bool fromRight, float gap) {
    if (barWidth >= width) {
        return barStart + ((barWidth - width) * 0.5F);
    }
    return fromRight ? barStart + barWidth - width - gap : barStart + gap;
}

float idleFor(const Box& box, const Box& picture) {
    return overlaps(box, picture) ? idleOverPicture : idleOnBlack;
}

/// A label centred in `box`.
Piece labelIn(const Box& box, Art art, std::vector<Button> lit, float idle) {
    const auto width = static_cast<float>(art.width);
    const auto height = static_cast<float>(art.height);
    return Piece{
        .box = {box.centreX() - (width * 0.5F), box.centreY() - (height * 0.5F), width, height},
        .art = std::move(art),
        .down = {},
        .lit = std::move(lit),
        .idle = idle,
        .lean = 0.0F};
}

void addStick(ControlPlan& plan, const Box& base, const ArcadePanel& panel, const Box& picture,
              const ArtSource& art) {
    const std::vector<Button> directions{Button::Up, Button::Down, Button::Left, Button::Right};
    const float idle = idleFor(base, picture);
    plan.pieces.push_back(Piece{.box = base,
                                .art = art.svg("stick_base.svg", base),
                                .down = {},
                                .lit = directions,
                                .idle = idle,
                                .lean = 0.0F});
    const float ball = base.width * ballShare;
    const Box knob{base.centreX() - (ball * 0.5F), base.centreY() - (ball * 0.5F), ball, ball};
    plan.pieces.push_back(Piece{.box = knob,
                                .art = art.svg("stick_ball.svg", knob),
                                .down = {},
                                .lit = directions,
                                .idle = idle,
                                .lean = base.width * leanShare});
    const float reach = base.width * 0.75F;
    plan.controls.push_back(
        Control{.hit = {base.centreX() - reach, base.centreY() - reach, reach * 2.0F, reach * 2.0F},
                .round = true,
                .stick = Stick{.x = base.centreX(),
                               .y = base.centreY(),
                               .deadZone = base.width * stickDeadZone,
                               .ways = panel.ways,
                               .vertical = panel.vertical},
                .targets = {}});
}

/// A small round button of its own: pause, Rotate screen.
void addRound(ControlPlan& plan, const Box& box, const char* file, Button button,
              const Box& picture, const ArtSource& art) {
    plan.pieces.push_back(Piece{.box = box,
                                .art = art.svg(file, box),
                                .down = {},
                                .lit = {button},
                                .idle = idleFor(box, picture),
                                .lean = 0.0F});
    plan.controls.push_back(Control{.hit = box.grown(box.width * 0.2F),
                                    .round = true,
                                    .stick = std::nullopt,
                                    .targets = {Target{box.centreX(), box.centreY(), button}}});
}

void addPill(ControlPlan& plan, const Box& box, const char* word, Button button,
             const gpu2d::Colour& colour, const Box& picture, float dp, const ArtSource& art,
             LabelFace& labels) {
    const std::array<svg::ColourSwap, 1> swap{svg::ColourSwap{artKey, colour}};
    const float idle = idleFor(box, picture);
    plan.pieces.push_back(Piece{.box = box,
                                .art = art.svg("arcade_pill.svg", box, swap),
                                .down = {},
                                .lit = {button},
                                .idle = idle,
                                .lean = 0.0F});
    plan.pieces.push_back(
        labelIn(box, labels.label(word, box.height * 0.48F, box.width * 0.82F), {button}, idle));
    plan.controls.push_back(Control{.hit = box.grown(pillSlack * dp),
                                    .round = false,
                                    .stick = std::nullopt,
                                    .targets = {Target{box.centreX(), box.centreY(), button}}});
}

/// The buttons' rows and the size they are drawn at.
struct ButtonRows {
    std::array<std::size_t, 2> rows{};
    float size{0.0F};
    float gap{0.0F};

    [[nodiscard]] std::size_t widest() const {
        return std::max(rows[0], rows[1]);
    }
    [[nodiscard]] float width() const {
        const auto columns = static_cast<float>(widest());
        return (columns * size) + ((columns - 1.0F) * gap);
    }
    /// How far the arc lifts the highest column, as a share of a button.
    [[nodiscard]] float deepest() const {
        const std::size_t columns = std::min(widest(), columnRise.size());
        return columns == 0 ? 0.0F
                            : *std::ranges::max_element(std::span{columnRise}.first(columns));
    }
    [[nodiscard]] float height() const {
        if (widest() == 0) {
            return 0.0F;
        }
        const float stacked = rows[1] != 0 ? (2.0F * size) + gap : size;
        return stacked + (size * deepest());
    }
};

/// The cabinet's rows for `panel`: a fighter's punches above its kicks.
std::array<std::size_t, 2> rowsOf(const ArcadePanel& panel) {
    const auto limbs = [&panel](Limb limb) {
        return static_cast<std::size_t>(
            std::ranges::count(panel.buttons, limb, &ArcadeButton::limb));
    };
    const std::size_t punches = limbs(Limb::Punch);
    const std::size_t kicks = limbs(Limb::Kick);
    if (punches != 0 && kicks != 0 && punches + kicks == panel.buttons.size()) {
        return {punches, kicks};
    }
    return rowsByCount.at(std::min(panel.buttons.size(), rowsByCount.size() - 1));
}

/// `panel`'s buttons for a `bar` wide, or over the picture up to `room` when the bar cannot hold
/// them.
ButtonRows rowsFor(const ArcadePanel& panel, float bar, float room, float dp) {
    ButtonRows rows;
    rows.rows = rowsOf(panel);
    rows.gap = buttonGap * dp;
    if (rows.widest() == 0) {
        return rows;
    }
    const auto columns = static_cast<float>(rows.widest());
    const float gaps = (columns - 1.0F) * rows.gap;
    const float onBar = (bar - gaps) / columns;
    const float across = onBar >= buttonSmallest * dp ? onBar : (room - gaps) / columns;
    rows.size = std::clamp(across, buttonSmallest * dp, buttonLargest * dp);
    return rows;
}

/// `button`'s face: its name, or its limb over its strength.
void addFace(ControlPlan& plan, const ArcadeButton& button, const Box& box, float idle,
             const ArtSource& art, LabelFace& labels) {
    if (button.limb == Limb::None) {
        const float type = button.label.size() <= 2 ? box.width * 0.42F : box.width * 0.24F;
        plan.pieces.push_back(labelIn(box, labels.label(button.label, type, box.width * 0.72F),
                                      {button.button}, idle));
        return;
    }
    const bool alone = button.label.empty();
    const float side = box.width * limbShare;
    const float lift = alone ? 0.0F : box.width * limbLift;
    const Box drawing{box.centreX() - (side * 0.5F), box.centreY() - (side * 0.5F) - lift, side,
                      side};
    plan.pieces.push_back(
        Piece{.box = drawing,
              .art = art.svg(button.limb == Limb::Punch ? "fist.svg" : "foot.svg", drawing),
              .down = {},
              .lit = {button.button},
              .idle = idle,
              .lean = 0.0F});
    if (!alone) {
        const Box under{box.x, box.y + (box.height * strengthDrop), box.width, box.height};
        plan.pieces.push_back(
            labelIn(under, labels.label(button.label, box.width * strengthShare, box.width * 0.5F),
                    {button.button}, idle));
    }
}

void addButtons(ControlPlan& plan, const ButtonRows& rows, float left, float top,
                const ArcadePanel& panel, const Box& picture, const ArtSource& art,
                LabelFace& labels) {
    Control cluster{.hit = {}, .round = false, .stick = std::nullopt, .targets = {}};
    std::size_t at = 0;
    const float rowStep = rows.size + rows.gap;
    for (std::size_t row = 0; row < rows.rows.size(); ++row) {
        for (std::size_t column = 0; column < rows.rows.at(row); ++column) {
            if (at >= panel.buttons.size()) {
                break;
            }
            const ArcadeButton& button = panel.buttons[at];
            const float rise = columnRise.at(std::min(column, columnRise.size() - 1));
            const Box box{left + (static_cast<float>(column) * rowStep),
                          top + (static_cast<float>(row) * rowStep) +
                              ((rows.deepest() - rise) * rows.size),
                          rows.size, rows.size};
            const std::array<svg::ColourSwap, 1> swap{
                svg::ColourSwap{artKey, buttonColours.at(at % buttonColours.size())}};
            const float idle = idleFor(box, picture);
            plan.pieces.push_back(Piece{.box = box,
                                        .art = art.svg("arcade_button.svg", box, swap),
                                        .down = art.svg("arcade_button_down.svg", box, swap),
                                        .lit = {button.button},
                                        .idle = idle,
                                        .lean = 0.0F});
            addFace(plan, button, box, idle, art, labels);
            cluster.targets.push_back(Target{box.centreX(), box.centreY(), button.button});
            ++at;
        }
    }
    cluster.hit = Box{left, top, rows.width(), rows.height()}.grown(rows.size * buttonSlack);
    if (!cluster.targets.empty()) {
        plan.controls.push_back(std::move(cluster));
    }
}

} // namespace

ControlPlan arcadeLayout(const WindowSurface& surface, const Box& picture, const ArcadePanel& panel,
                         const ArtSource& art, LabelFace& labels) {
    const ButtonRows largest{.rows = rowsOf(panel), .size = buttonLargest, .gap = buttonGap};
    const ButtonRows smallest{.rows = largest.rows, .size = buttonSmallest, .gap = buttonGap};
    const float pills = pillDrop + (2.0F * pillHeight) + pillGap;
    const bool upright = surface.upright();
    const float tall = std::max(stickLargest, largest.height() + pills) + bottomMargin + pauseSize +
                       (2.0F * inset);
    const float dp = surface.dpWithin((panel.ways != 0 ? stickSmallest + inset : 0.0F) +
                                          (2.0F * inset) + smallest.width(),
                                      upright ? tall / uprightShare : tall);
    const float gap = inset * dp;
    const float left = surface.safeX;
    const float top = surface.safeY;
    const float right = surface.safeX + surface.safeWidth;
    const float bottom = surface.safeY + surface.safeHeight;
    // Upright the panel stands under the picture, so there are no bars beside it.
    const float leftBar = upright ? 0.0F : std::max(0.0F, picture.x - left);
    const float rightStart = upright ? right : std::min(right, picture.x + picture.width);
    const float rightBar = std::max(0.0F, right - rightStart);

    const float stickSide =
        std::clamp(leftBar - (2.0F * gap), stickSmallest * dp, stickLargest * dp);
    const float stickTaken = panel.ways != 0 ? stickSide + gap : 0.0F;
    const float buttonRoom = surface.safeWidth - stickTaken - (2.0F * gap);
    const ButtonRows rows = rowsFor(panel, rightBar, buttonRoom, dp);
    const float panelHeight = std::max(panel.ways != 0 ? stickSide : 0.0F, rows.height());
    const float middleY = bottom - (bottomMargin * dp) - (panelHeight * 0.5F);

    const float buttonsLeft = placed(rows.width(), rightStart, rightBar, true, gap);
    const float buttonsTop = middleY - (rows.height() * 0.5F);

    // Stacked when the bar cannot take them side by side.
    const float pillRoom = upright ? pillWidth * dp : rightBar - (2.0F * gap);
    const float pillW = std::min(pillWidth * dp, std::max(pillRoom, 54.0F * dp));
    const float pillH = pillHeight * dp;
    const float pillStep = pillGap * dp;
    const bool sideBySide = upright || rightBar >= (2.0F * pillW) + pillStep + (2.0F * gap);
    const float pillsWidth = sideBySide ? (2.0F * pillW) + pillStep : pillW;
    const float pillsLeft = placed(pillsWidth, rightStart, rightBar, true, gap);
    const float pillsBottom = buttonsTop - (pillDrop * dp);
    const Box start{sideBySide ? pillsLeft + pillW + pillStep : pillsLeft, pillsBottom - pillH,
                    pillW, pillH};
    const Box coin{pillsLeft, sideBySide ? start.y : start.y - pillH - pillStep, pillW, pillH};

    const float roundSide = pauseSize * dp;
    const float panelTop = middleY - (panelHeight * 0.5F);
    const float roundsY = upright ? std::min(coin.y, panelTop - gap - roundSide) : top + gap;
    const float roundsLeft = placed((2.0F * roundSide) + gap, left, leftBar, false, gap);
    const Box pause{roundsLeft, roundsY, roundSide, roundSide};
    const Box turn{roundsLeft + roundSide + gap, roundsY, roundSide, roundSide};

    ControlPlan plan;
    plan.band = upright ? static_cast<float>(surface.height) - roundsY + gap : 0.0F;
    addRound(plan, pause, "pause.svg", Button::Guide, picture, art);
    addRound(plan, turn, "rotate.svg", Button::Rotate, picture, art);
    addPill(plan, coin, "COIN", Button::Select, coinColour, picture, dp, art, labels);
    addPill(plan, start, "START", Button::Start, startColour, picture, dp, art, labels);

    addButtons(plan, rows, buttonsLeft, buttonsTop, panel, picture, art, labels);

    if (panel.ways != 0) {
        const Box base{placed(stickSide, left, leftBar, false, gap), middleY - (stickSide * 0.5F),
                       stickSide, stickSide};
        addStick(plan, base, panel, picture, art);
    }
    return plan;
}

} // namespace opensu::touch
