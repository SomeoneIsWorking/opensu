// page_frame — the frame of an interface-toolkit screen that fills the window: room for the shell's
// top bar, the body (a column or a row of panes), then the hint bar; and the pane a list of rows
// sits on, and the column of tabs beside it. Settings, Devices, Downloads and Giveaways share it. A
// tall card over the dimmed screen (search, the folder chooser) has its own frame here.
#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "screen_stack.hpp"

namespace opensu::ui {

/// The widest a page's body is, in dp.
inline constexpr float pageMostDp = 1280.0f;

template <typename Body> struct PageFrame {
    std::unique_ptr<gpu2d::ui::Backdrop> root;
    Body* body{};
};

/// The frame under `chrome`, with a `Body` (`Column` or `Row`) between the top bar and the hints,
/// on `root`.
template <typename Body>
[[nodiscard]] PageFrame<Body>
pageFrame(const gpu2d::ui::Theme& theme, const Chrome& chrome,
          std::unique_ptr<gpu2d::ui::Backdrop> root = std::make_unique<gpu2d::ui::Backdrop>()) {
    PageFrame<Body> frame{std::move(root), nullptr};
    frame.root->space(theme.spacing.gap);
    frame.root->template add<gpu2d::ui::Spacer>(0.0f).fixHeight(chrome.top / theme.scale);
    Body& body = frame.root->template add<Body>();
    if constexpr (std::is_same_v<Body, gpu2d::ui::Row>) {
        body.align(gpu2d::ui::Align::Stretch);
    }
    // A wide screen centres the page instead of stretching its rows.
    const float side =
        std::max(theme.spacing.pad * 2.0f, (chrome.width / theme.scale - pageMostDp) * 0.5f);
    body.pad(gpu2d::ui::Insets{side, 0.0f, side, 0.0f}).space(theme.spacing.pad);
    body.grow();
    frame.body = &body;
    frame.root->template add<gpu2d::ui::HintBar>();
    return frame;
}

/// The root of a card `widthDp` wide standing over the dimmed screen from under `chrome` to the
/// bottom of the viewport, and the card.
struct CardFrame {
    std::unique_ptr<gpu2d::ui::Widget> root;
    gpu2d::ui::Card* card{};
};
[[nodiscard]] CardFrame tallCard(const gpu2d::ui::Theme& theme, const Chrome& chrome,
                                 float widthDp);

/// A pane growing across `parent` with a list `id` of rows on it.
[[nodiscard]] gpu2d::ui::List& rowsPane(gpu2d::ui::Container& parent, const gpu2d::ui::Theme& theme,
                                        std::string id);

/// Whether a page under `chrome` is too narrow for a side column beside its rows (a portrait
/// screen): it then stands one part above the next.
[[nodiscard]] bool narrow(const gpu2d::ui::Theme& theme, const Chrome& chrome) noexcept;

/// A page of tabs `id` and what the chosen one shows: the tabs a scrolling column on the left,
/// or on a narrow page a strip across the top; `body` is where the caller adds what they show.
struct TabbedPage {
    std::unique_ptr<gpu2d::ui::Backdrop> root;
    gpu2d::ui::Container* body{};
};
[[nodiscard]] TabbedPage tabbedPage(const gpu2d::ui::Theme& theme, const Chrome& chrome,
                                    const std::string& id, std::vector<gpu2d::ui::Tabs::Tab> tabs,
                                    std::size_t chosen, std::function<void(std::size_t)> chose);

} // namespace opensu::ui
