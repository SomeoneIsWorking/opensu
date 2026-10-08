// launch_panel — what the shell shows while a game it launched has no window yet.
//
// iideck's own; iiSU hands off to an Android app that is on screen at once. Pure state, so it is
// tested without a window.
#pragma once

#include <optional>
#include <string>

namespace iideck::ui {

class LaunchPanel {
  public:
    /// Opens over `title`, at "Starting".
    void open(std::string title);
    /// The stage's line, and how far through it when it has a measure.
    void update(std::string line, std::optional<double> fraction);
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

  private:
    std::string title_;
    std::string line_;
    std::optional<double> fraction_;
    bool open_{false};
};

} // namespace iideck::ui
