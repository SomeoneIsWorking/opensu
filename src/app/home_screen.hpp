// home_screen — Home as the navigator's root: the shelf's tiles in a `gpu2d::ui::CellGrid`, and
// what the pad does on them. The D-pad, the pointer and the wheel move the grid's focus through
// the cells; A, a click on a tile, Y, X, Select (or a right click), Search and Start go to the
// app's hooks; LB and RB reach the dock in the chrome. Back never closes it: it backs out of a
// search or a folder. Over a running game it shows nothing.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "ui/home_grid.hpp"

namespace opensu::app {

class HomeScreen final : public gpu2d::ui::Screen {
  public:
    /// The grid's widget, which holds the focus on Home.
    static constexpr const char* tilesId = "home/tiles";

    struct Hooks {
        /// Whether a game runs, which leaves Home empty.
        std::function<bool()> inGame;
        /// A on the focused tile.
        std::function<void()> press;
        /// Y: the focused game's details.
        std::function<void()> details;
        /// X: list the stores again.
        std::function<void()> refresh;
        /// Select or a right click: the focused tile's menu.
        std::function<void()> options;
        std::function<void()> search;
        /// Start: the layout picker.
        std::function<void()> menu;
        /// B: out of the search or the folder.
        std::function<void()> back;
    };

    HomeScreen(ui::HomeGrid& grid, Hooks hooks)
        : Screen{"home", "Home"}, grid_{grid}, hooks_{std::move(hooks)} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    /// The corner hints stand for Home's prompts.
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override {
        return {};
    }
    [[nodiscard]] std::string initialFocus() const override {
        return tilesId;
    }

  private:
    [[nodiscard]] bool inGame() const;

    ui::HomeGrid& grid_;
    Hooks hooks_;
};

} // namespace opensu::app
