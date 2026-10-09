#include "details_controller.hpp"

#include <algorithm>
#include <chrono>

namespace opensu::app {

ui::DetailsView DetailsController::viewOf(const library::Game& game) const {
    return ui::detailsFor(game, preferences_.values().hidden.contains(game),
                          std::chrono::system_clock::now());
}

void DetailsController::open() {
    const library::Game* game = hooks_.focused();
    if (game == nullptr) {
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    page_.open(viewOf(*game));
}

void DetailsController::close() {
    if (!page_.isOpen()) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    page_.close();
}

void DetailsController::refresh() {
    if (!page_.isOpen()) {
        return;
    }
    if (const std::optional<library::Game> game = hooks_.find(page_.view().gameId)) {
        page_.refresh(viewOf(*game));
    } else {
        page_.close();
    }
}

void DetailsController::act(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (page_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        if (page_.selected() == ui::DetailsAction::Emulator) {
            if (const std::optional<library::Game> game = hooks_.find(page_.view().gameId)) {
                cycleEmulator(*game, button == gamepad::Button::Right ? 1 : -1);
            }
        }
        break;
    case gamepad::Button::A:
        run(page_.selected());
        break;
    case gamepad::Button::B:
        close();
        break;
    default:
        break;
    }
}

void DetailsController::run(ui::DetailsAction action) {
    if (action == ui::DetailsAction::Back) {
        close();
        return;
    }
    const std::optional<library::Game> game = hooks_.find(page_.view().gameId);
    if (!game) {
        close();
        return;
    }
    switch (action) {
    case ui::DetailsAction::Launch:
        hooks_.launch(*game);
        break;
    case ui::DetailsAction::Emulator:
        cycleEmulator(*game, 1);
        break;
    case ui::DetailsAction::Hidden:
        hooks_.setHidden(*game, !preferences_.values().hidden.contains(*game));
        // The game leaves or joins the shelf the page was opened over.
        close();
        break;
    case ui::DetailsAction::Back:
        break;
    }
}

void DetailsController::cycleEmulator(const library::Game& game, int delta) {
    const auto& options = game.emulatorOptions;
    if (options.size() < 2) {
        return;
    }
    const auto at = std::ranges::find(options, game.emulator, &library::EmulatorOption::name);
    const auto from = at == options.end() ? 0 : static_cast<int>(at - options.begin());
    const auto count = static_cast<int>(options.size());
    const auto next = static_cast<std::size_t>((from + delta + count) % count);
    sounds_.play(audio::Effect::Navigation);
    hooks_.chooseEmulator(game, options[next].name);
}

} // namespace opensu::app
