// page_frame — the frame of an interface-toolkit screen that fills the window: room for the shell's
// top bar, the body (a column or a row of panes), then the hint bar; and the pane a list of rows
// sits on. Settings, Devices, Downloads and Giveaways share it.
#pragma once

#include <memory>
#include <string>
#include <type_traits>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "screen_stack.hpp"

namespace opensu::ui {

template <typename Body> struct PageFrame {
    std::unique_ptr<gpu2d::ui::Backdrop> root;
    Body* body{};
};

/// The frame under `chrome`, with a `Body` (`Column` or `Row`) between the top bar and the hints.
template <typename Body>
[[nodiscard]] PageFrame<Body> pageFrame(const gpu2d::ui::Theme& theme, const Chrome& chrome) {
    PageFrame<Body> frame{std::make_unique<gpu2d::ui::Backdrop>(), nullptr};
    frame.root->space(theme.spacing.gap);
    frame.root->template add<gpu2d::ui::Spacer>(0.0f).fixHeight(chrome.top / theme.scale);
    Body& body = frame.root->template add<Body>();
    if constexpr (std::is_same_v<Body, gpu2d::ui::Row>) {
        body.align(gpu2d::ui::Align::Stretch);
    }
    body.pad(gpu2d::ui::Insets{theme.spacing.pad * 2.0f, 0.0f, theme.spacing.pad * 2.0f, 0.0f})
        .space(theme.spacing.pad);
    body.grow();
    frame.body = &body;
    frame.root->template add<gpu2d::ui::HintBar>().fixHeight(theme.spacing.hintBarHeight);
    return frame;
}

/// A pane growing across `parent` with a list `id` of rows on it.
[[nodiscard]] gpu2d::ui::List& rowsPane(gpu2d::ui::Container& parent, const gpu2d::ui::Theme& theme,
                                        std::string id);

} // namespace opensu::ui
