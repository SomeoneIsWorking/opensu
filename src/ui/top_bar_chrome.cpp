#include "top_bar_chrome.hpp"

#include <vector>

#include "chrome_panel.hpp"

namespace opensu::ui {

std::string TopBarChrome::crumbId(std::size_t index) {
    return "topbar/crumb/" + std::to_string(index);
}

std::string TopBarChrome::launcherId(std::size_t index) {
    return "topbar/launcher/" + std::to_string(index);
}

std::unique_ptr<gpu2d::ui::Widget> TopBarChrome::build(const gpu2d::ui::Theme& /*theme*/) {
    auto root = std::make_unique<ChromePanel>([this](gpu2d::ui::Painter& painter) {
        paint(painter);
    });
    if (live_.crumbs) {
        const std::vector<Rect> cells = hud_.crumbCells();
        for (std::size_t i = 0; i < cells.size(); ++i) {
            root->part(std::make_unique<ChromePart>(crumbId(i),
                                                    [this, i](gpu2d::ui::ActContext& /*context*/) {
                                                        if (hooks_.crumb) {
                                                            hooks_.crumb(i);
                                                        }
                                                    }),
                       onCanvas(cells[i]));
        }
    }
    if (live_.launchers) {
        const std::vector<Hud::BadgeCell> badges = hud_.badgeCells();
        for (std::size_t i = 0; i < badges.size(); ++i) {
            root->part(std::make_unique<ChromePart>(
                           launcherId(i),
                           [this, badge = badges[i].badge](gpu2d::ui::ActContext& /*context*/) {
                               if (hooks_.launcher) {
                                   hooks_.launcher(badge);
                               }
                           }),
                       onCanvas(badges[i].hit));
        }
    }
    return root;
}

void TopBarChrome::paint(gpu2d::ui::Painter& painter) {
    Hud::TopBarHover hover;
    const std::size_t crumbs = hud_.crumbCells().size();
    for (std::size_t i = 0; i < crumbs; ++i) {
        if (painter.hovered == crumbId(i)) {
            hover.crumb = i;
        }
    }
    const std::vector<Hud::BadgeCell> badges = hud_.badgeCells();
    for (std::size_t i = 0; i < badges.size(); ++i) {
        if (painter.hovered == launcherId(i)) {
            hover.launcher = badges[i].badge.source;
        }
    }
    hud_.drawTopBar(painter.canvas, hover);
}

} // namespace opensu::ui
