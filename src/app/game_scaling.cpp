#include "game_scaling.hpp"

#include "library/emulator_choice.hpp"
#include "lucent/log.h"

namespace opensu::app {
namespace {

/// Gamescope's sharpness when it starts (`--sharpness`'s default).
constexpr std::uint32_t gamescopeDefaultSharpness = 2;

} // namespace

GameScaling::GameScaling(const GameSettings& gameSettings, std::unique_ptr<ScalingOutput> output)
    : gameSettings_{gameSettings}, output_{std::move(output)},
      applied_{.sharpness = gamescopeDefaultSharpness} {
}

void GameScaling::setScreen(settings::ScreenSize screen) {
    screen_ = screen;
    applied_.size = screen;
    lucent::info("scaling", "the screen is {} x {}", screen.width, screen.height);
}

void GameScaling::follow(const library::Game* running) {
    if (!output_ || !screen_) {
        return;
    }
    const bool same = running == nullptr ? !running_ : running_ && running_->id == running->id;
    if (same) {
        return;
    }
    if (running == nullptr) {
        running_.reset();
    } else {
        running_ = *running;
    }
    apply(wanted(running, *screen_));
}

void GameScaling::changed() {
    if (!output_ || !screen_ || !running_) {
        return;
    }
    Applied next = wanted(&*running_, *screen_);
    // A game reads its screen's size as it starts; a new one waits for its next launch.
    next.size = applied_.size;
    apply(next);
}

GameScaling::Applied GameScaling::wanted(const library::Game* game,
                                         settings::ScreenSize screen) const {
    if (game == nullptr) {
        return Applied{.size = screen, .sharpness = gamescopeDefaultSharpness};
    }
    const settings::ScalingSettings resolved =
        settings::resolveScaling(gameSettings_.layers(), library::layerKeysOf(*game));
    return Applied{.size = resolved.renderSize(screen),
                   .filter = resolved.filter,
                   .sharpness = resolved.gamescopeSharpness()};
}

void GameScaling::apply(const Applied& next) {
    if (next == applied_) {
        return;
    }
    const std::string title = running_ ? running_->title : std::string{"no game"};
    if (next.size != applied_.size) {
        output_->resize(next.size);
        lucent::info("scaling", "{}: games see a {} x {} screen", title, next.size.width,
                     next.size.height);
    }
    if (next.filter != applied_.filter || next.sharpness != applied_.sharpness) {
        output_->upscale(next.filter, next.sharpness);
        lucent::info("scaling", "{}: upscaled with {}, Gamescope sharpness {}", title,
                     settings::scalingFilterChoices[static_cast<std::size_t>(next.filter)].key,
                     next.sharpness);
    }
    applied_ = next;
}

} // namespace opensu::app
