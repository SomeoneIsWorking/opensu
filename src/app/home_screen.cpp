#include "home_screen.hpp"

#include "gpu2d/ui/cell_grid.hpp"
#include "gpu2d/ui/layout.hpp"

namespace opensu::app {
namespace {

using gpu2d::ui::Action;

void call(const std::function<void()>& hook) {
    if (hook) {
        hook();
    }
}

} // namespace

bool HomeScreen::inGame() const {
    return hooks_.inGame && hooks_.inGame();
}

std::unique_ptr<gpu2d::ui::Widget> HomeScreen::build(const gpu2d::ui::Theme& /*theme*/) {
    if (inGame()) {
        return std::make_unique<gpu2d::ui::Column>();
    }
    return std::make_unique<gpu2d::ui::CellGrid>(tilesId, grid_);
}

gpu2d::ui::Screen::BackReply HomeScreen::back(gpu2d::ui::ActContext& /*context*/) {
    if (!inGame()) {
        call(hooks_.back);
    }
    return BackReply::Stay;
}

bool HomeScreen::act(Action action, gpu2d::ui::ActContext& /*context*/) {
    // Over a running game only the screens the Guide menu opened take buttons.
    if (inGame()) {
        return false;
    }
    switch (action) {
    case Action::Accept:
        call(hooks_.press);
        return true;
    case Action::Tertiary:
        call(hooks_.details);
        return true;
    case Action::Secondary:
        call(hooks_.refresh);
        return true;
    case Action::Options:
        call(hooks_.options);
        return true;
    case Action::Search:
        call(hooks_.search);
        return true;
    case Action::Menu:
        call(hooks_.menu);
        return true;
    default:
        return false;
    }
}

} // namespace opensu::app
