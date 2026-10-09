// launch_panel — the card the shell shows over the grid while a game is on its way: launching
// until its window shows, asking whether to install it, or installing it.
//
// opensu's own; iiSU hands off to an Android app that is on screen at once. Pure state, so it is
// tested without a window.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"

namespace opensu::ui {

/// A button and what it does, shown along the card's foot.
struct PanelHint {
    std::string button;
    std::string action;

    bool operator==(const PanelHint&) const = default;
};

/// The hint glyphs' size in dp.
inline constexpr float panelGlyphDp = 20.0f;

/// What the card's layout is made from: the heights and widths the painter measures, in pixels.
struct PanelMetrics {
    /// The title line's and the status line's heights.
    float titleBox{};
    float lineBox{};
    /// Each hint's width, glyph through text.
    std::vector<float> hintWidths;
};

/// Where the card and its lines stand, in pixels; the lines are given by their centres.
struct PanelLayout {
    Rect card;
    float padding{};
    float centreX{};
    float titleY{};
    float lineY{};
    /// The progress bar's or the dots' centre, and their height.
    float meterY{};
    float meter{};
    /// The hints' centre line and the glyph size.
    float hintY{};
    float glyph{};
    /// Each hint's content, left to right.
    std::vector<Rect> hints;
    /// The room between hints.
    float hintSpacing{};

    /// The hint under the point, or nothing: a hint takes the room beside it up to its
    /// neighbour's, and a little above and below.
    [[nodiscard]] std::optional<std::size_t> hintAt(float x, float y) const noexcept;
};

/// The card in a `width` x `height` frame at `dp` pixels per dp.
[[nodiscard]] PanelLayout layoutPanel(float width, float height, float dp,
                                      const PanelMetrics& metrics);

class LaunchPanel {
  public:
    /// Opens over `title`, at "Starting", with B to cancel.
    void open(std::string title);
    /// The buttons the card offers.
    void setHints(std::vector<PanelHint> hints);
    /// The stage's line, and how far through it when it has a measure. A stage that is not
    /// `busy` waits on the player and shows no activity.
    void update(std::string line, std::optional<double> fraction, bool busy = true);
    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }
    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }
    [[nodiscard]] const std::string& line() const noexcept {
        return line_;
    }
    /// 0 to 1, when the stage has a measure.
    [[nodiscard]] std::optional<double> fraction() const noexcept {
        return fraction_;
    }
    [[nodiscard]] bool busy() const noexcept {
        return busy_;
    }
    [[nodiscard]] const std::vector<PanelHint>& hints() const noexcept {
        return hints_;
    }

  private:
    std::string title_;
    std::string line_;
    std::optional<double> fraction_;
    std::vector<PanelHint> hints_;
    bool busy_{true};
    bool open_{false};
};

} // namespace opensu::ui
