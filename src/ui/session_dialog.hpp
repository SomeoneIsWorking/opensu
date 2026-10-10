// session_dialog — the modal that explains a system change and asks for consent: a title, a few
// paragraphs and two buttons, Accept and Cancel. While the change runs it shows one line of
// progress instead of the buttons. Pure state, so it is tested without a window.
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "grid_focus.hpp"
#include "home_layout.hpp"
#include "panel_frame.hpp"

namespace opensu::ui {

/// What the dialog says and offers.
struct SessionDialogText {
    std::string title;
    std::vector<std::string> paragraphs;
    std::string accept{"Accept"};
    std::string cancel{"Cancel"};
};

/// The buttons, by their place in the dialog.
inline constexpr std::size_t dialogAccept = 0;
inline constexpr std::size_t dialogCancel = 1;

/// Where the dialog stands in a frame, in pixels.
struct SessionDialogLayout {
    Rect card;
    float radius{};
    float padding{};
    /// The title's centre line.
    float titleY{};
    /// Where the wrapped paragraphs start, and how wide they may run.
    Rect body;
    /// Accept and Cancel; both are empty rectangles while a change runs.
    std::array<Rect, 2> buttons;
    /// The progress line's centre line while a change runs.
    float progressY{};

    /// The button under the point, or nothing.
    [[nodiscard]] std::optional<std::size_t> buttonAt(float x, float y) const noexcept;
};

/// The width the paragraphs wrap to in a frame `frameWidth` wide at `dp` pixels per dp.
[[nodiscard]] float dialogBodyWidth(float frameWidth, float dp) noexcept;

/// The heights the dialog is laid out from: the title's and a line's boxes and the wrapped
/// paragraphs'.
struct SessionDialogMetrics {
    float titleBox{};
    float lineBox{};
    float bodyHeight{};
};

/// The dialog in `frame`, with the buttons or, while a change runs, a progress line.
[[nodiscard]] SessionDialogLayout
layoutSessionDialog(const PanelFrame& frame, const SessionDialogMetrics& metrics, bool buttons);

class SessionDialog {
  public:
    /// Opens with the text, Accept focused.
    void open(SessionDialogText text);
    /// Replaces the buttons with `line` while the change runs; B and A do nothing then.
    void showProgress(std::string line);
    void close() noexcept {
        open_ = false;
        progress_.reset();
    }

    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }
    [[nodiscard]] const SessionDialogText& text() const noexcept {
        return text_;
    }
    /// The progress line, when a change runs.
    [[nodiscard]] const std::optional<std::string>& progress() const noexcept {
        return progress_;
    }
    [[nodiscard]] bool busy() const noexcept {
        return progress_.has_value();
    }

    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    /// Left and Right (or Up and Down) switch between the buttons; reports whether focus moved.
    bool move(Direction direction) noexcept;
    bool focusButton(std::size_t index) noexcept;

  private:
    SessionDialogText text_;
    std::optional<std::string> progress_;
    std::size_t focus_{dialogAccept};
    bool open_{false};
};

} // namespace opensu::ui
