// launch_panel — the card the shell shows over the grid while a game is on its way: launching
// until its window shows, asking whether to install it, or installing it.
//
// iideck's own; iiSU hands off to an Android app that is on screen at once. Pure state, so it is
// tested without a window.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace iideck::ui {

/// A button and what it does, shown along the card's foot.
struct PanelHint {
    std::string button;
    std::string action;

    bool operator==(const PanelHint&) const = default;
};

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

} // namespace iideck::ui
