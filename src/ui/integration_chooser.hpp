// integration_chooser — the first-run card: what openSU can work with besides the ROM library and
// the libretro runner, each with a switch, what it does and what it needs, and Done. Pure state
// and geometry, tested without a window; `IntegrationChooserPainter` draws it and
// `app::FirstRunChooser` fills and acts on it.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"
#include "panel_frame.hpp"
#include "settings_page.hpp"

namespace opensu::ui {

/// One integration on the card.
struct IntegrationOption {
    std::string id;
    std::string label;
    /// What it does and what it needs, in one line.
    std::string note;
    bool on{false};

    bool operator==(const IntegrationOption&) const = default;
};

/// Where the card stands in a frame, in pixels.
struct IntegrationChooserLayout {
    Rect card;
    float radius{};
    float padding{};
    /// The title's centre line.
    float titleY{};
    /// Where the wrapped introduction starts, and how wide it may run.
    Rect intro;
    std::vector<SettingsRowBox> rows;
    Rect done;

    /// The option under the point, `rows.size()` for Done, or nothing.
    [[nodiscard]] std::optional<std::size_t> itemAt(float x, float y) const noexcept;
};

/// The width the introduction wraps to in a frame `frameWidth` wide at `dp` pixels per dp.
[[nodiscard]] float chooserIntroWidth(float frameWidth, float dp) noexcept;

/// The card in `frame` for `rows` options, with a title `titleBox` and an introduction
/// `introHeight` tall; the rows shrink to keep the card inside the frame.
[[nodiscard]] IntegrationChooserLayout layoutIntegrationChooser(const PanelFrame& frame,
                                                                float titleBox, float introHeight,
                                                                std::size_t rows);

class IntegrationChooser {
  public:
    /// Opens with `options`, the first focused.
    void open(std::vector<IntegrationOption> options);
    void close() noexcept {
        open_ = false;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }
    [[nodiscard]] const std::vector<IntegrationOption>& options() const noexcept {
        return options_;
    }
    /// The focused item: an option, or `options().size()` for Done.
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    [[nodiscard]] bool onDone() const noexcept {
        return focus_ == options_.size();
    }
    /// Moves the focus `delta` items, stopping at the ends; reports whether it moved.
    bool move(int delta) noexcept;
    /// Focuses item `index`; reports whether it moved.
    bool focusItem(std::size_t index) noexcept;
    /// Flips the focused option's switch; false on Done.
    bool toggleFocused() noexcept;

    /// The options as rows of switches, as the painter draws them.
    [[nodiscard]] std::vector<SettingsRow> rows() const;

  private:
    std::vector<IntegrationOption> options_;
    std::size_t focus_{0};
    bool open_{false};
};

} // namespace opensu::ui
