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
constexpr float stickSmallest = 72.0F;
constexpr float stickLargest = 150.0F;
/// A finger takes the stick this far from its centre at the least, past what a narrow bar draws.
constexpr float stickReach = 92.0F;
constexpr float stickDeadZone = 0.12F;
constexpr float ballShare = 0.5F;
constexpr float leanShare = 0.2F;
constexpr float buttonSmallest = 52.0F;
constexpr float buttonLargest = 70.0F;
constexpr float buttonGap = 8.0F;
constexpr float buttonSlack = 0.2F;
/// How far above the panel's first column each column sits, as a share of a button: the arc a
/// hand makes.
constexpr std::array<float, 4> columnRise{0.0F, 0.26F, 0.34F, 0.26F};
constexpr float pillWidth = 68.0F;
constexpr float pillHeight = 32.0F;
constexpr float pillGap = 8.0F;
constexpr float pillSlack = 6.0F;
constexpr float pauseSize = 40.0F;
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
              float dp, const ArtSource& art) {
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
    const float reach = std::max(base.width * 0.75F, stickReach * dp);
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

ButtonRows rowsFor(std::size_t count, float bar, float dp) {
    ButtonRows rows;
    rows.rows = rowsByCount.at(std::min(count, rowsByCount.size() - 1));
    rows.gap = buttonGap * dp;
    if (rows.widest() == 0) {
        return rows;
    }
    const auto columns = static_cast<float>(rows.widest());
    const float room = (bar - (2.0F * inset * dp) - ((columns - 1.0F) * rows.gap)) / columns;
    rows.size = std::clamp(room, buttonSmallest * dp, buttonLargest * dp);
    return rows;
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
            const float type = button.label.size() <= 2 ? rows.size * 0.42F : rows.size * 0.24F;
            plan.pieces.push_back(labelIn(box, labels.label(button.label, type, rows.size * 0.72F),
                                          {button.button}, idle));
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
    const float dp = surface.dp;
    const float gap = inset * dp;
    const float left = surface.safeX;
    const float top = surface.safeY;
    const float right = surface.safeX + surface.safeWidth;
    const float bottom = surface.safeY + surface.safeHeight;
    const float leftBar = std::max(0.0F, picture.x - left);
    const float rightStart = std::min(right, picture.x + picture.width);
    const float rightBar = std::max(0.0F, right - rightStart);

    const float stickSide =
        std::clamp(leftBar - (2.0F * gap), stickSmallest * dp, stickLargest * dp);
    const ButtonRows rows = rowsFor(panel.buttons.size(), rightBar, dp);
    const float panelHeight = std::max(panel.ways != 0 ? stickSide : 0.0F, rows.height());
    const float middleY = bottom - (bottomMargin * dp) - (panelHeight * 0.5F);

    ControlPlan plan;
    const float pauseSide = pauseSize * dp;
    const Box pause{placed(pauseSide, left, leftBar, false, gap), top + gap, pauseSide, pauseSide};
    plan.pieces.push_back(Piece{.box = pause,
                                .art = art.svg("pause.svg", pause),
                                .down = {},
                                .lit = {Button::Guide},
                                .idle = idleFor(pause, picture),
                                .lean = 0.0F});
    plan.controls.push_back(
        Control{.hit = pause.grown(pauseSide * 0.2F),
                .round = true,
                .stick = std::nullopt,
                .targets = {Target{pause.centreX(), pause.centreY(), Button::Guide}}});

    const float buttonsLeft = placed(rows.width(), rightStart, rightBar, true, gap);
    const float buttonsTop = middleY - (rows.height() * 0.5F);

    // Stacked when the bar cannot take them side by side.
    const float pillW = std::min(pillWidth * dp, std::max(rightBar - (2.0F * gap), 54.0F * dp));
    const float pillH = pillHeight * dp;
    const float pillStep = pillGap * dp;
    const bool sideBySide = rightBar >= (2.0F * pillW) + pillStep + (2.0F * gap);
    const float pillsWidth = sideBySide ? (2.0F * pillW) + pillStep : pillW;
    const float pillsLeft = placed(pillsWidth, rightStart, rightBar, true, gap);
    const float pillsBottom = buttonsTop - (14.0F * dp);
    const Box start{sideBySide ? pillsLeft + pillW + pillStep : pillsLeft, pillsBottom - pillH,
                    pillW, pillH};
    const Box coin{pillsLeft, sideBySide ? start.y : start.y - pillH - pillStep, pillW, pillH};
    addPill(plan, coin, "COIN", Button::Select, coinColour, picture, dp, art, labels);
    addPill(plan, start, "START", Button::Start, startColour, picture, dp, art, labels);

    addButtons(plan, rows, buttonsLeft, buttonsTop, panel, picture, art, labels);

    if (panel.ways != 0) {
        const Box base{placed(stickSide, left, leftBar, false, gap), middleY - (stickSide * 0.5F),
                       stickSide, stickSide};
        addStick(plan, base, panel, picture, dp, art);
    }
    return plan;
}

} // namespace opensu::touch
