#include "game_presentation.hpp"

#include "library/emulator_choice.hpp"
#include "lucent/log.h"

namespace opensu::app {
namespace {

/// Gamescope's sharpness when it starts (`--sharpness`'s default).
constexpr std::uint32_t gamescopeDefaultSharpness = 2;

} // namespace

bool GamePresentation::FollowedGame::take(const library::Game* game) {
    const bool same = game == nullptr ? !game_ : game_ && game_->id == game->id;
    if (same) {
        return false;
    }
    if (game == nullptr) {
        game_.reset();
    } else {
        game_ = *game;
    }
    return true;
}

GamePresentation::GamePresentation(const GameSettings& gameSettings,
                                   std::unique_ptr<PresentationOutput> output,
                                   std::function<std::optional<std::filesystem::path>()> findDll)
    : gameSettings_{gameSettings}, output_{std::move(output)}, findDll_{std::move(findDll)},
      upscale_{Upscale{.sharpness = gamescopeDefaultSharpness}} {
    if (supported()) {
        dll_ = findDll_();
    }
}

void GamePresentation::setScreen(settings::ScreenSize screen) {
    screen_ = screen;
    size_.wrote(screen);
    lucent::info("scaling", "the screen is {} x {}", screen.width, screen.height);
}

void GamePresentation::follow(const library::Game* inFront, const library::Game* running) {
    if (!output_) {
        return;
    }
    if (front_.take(inFront)) {
        if (inFront != nullptr) {
            dll_ = findDll_();
        }
        applyFrameGen();
    } else if (!frameGen_.written()) {
        applyFrameGen();
    }
    if (screen_ && running_.take(running)) {
        applyScaling(*screen_, true);
    }
}

void GamePresentation::changed() {
    if (!output_) {
        return;
    }
    applyFrameGen();
    if (screen_ && running_.get() != nullptr) {
        // A game reads its screen's size as it starts; a new one waits for its next launch.
        applyScaling(*screen_, false);
    }
}

std::optional<FrameGenRequest> GamePresentation::wantedFrameGen() const {
    const library::Game* front = front_.get();
    if (front == nullptr || !dll_) {
        return std::nullopt;
    }
    const settings::FrameGenSettings resolved =
        settings::resolveFrameGen(gameSettings_.layers(), library::layerKeysOf(*front));
    if (resolved.mode == settings::FrameGenMode::Off) {
        return std::nullopt;
    }
    return FrameGenRequest{resolved, *dll_};
}

void GamePresentation::applyFrameGen() {
    std::optional<FrameGenRequest> request = wantedFrameGen();
    if (!frameGen_.differs(request)) {
        return;
    }
    const library::Game* front = front_.get();
    const std::string title = front != nullptr ? front->title : std::string{};
    if (!request) {
        output_->stopGenerating();
        lucent::info("framegen", "frame generation off{}",
                     front != nullptr ? " for " + title + (dll_ ? "" : ": no Lossless.dll")
                                      : std::string{": no game in front"});
    } else {
        output_->generate(*request);
        const settings::FrameGenSettings& on = request->settings;
        lucent::info("framegen",
                     "frame generation for {}: {}, ratio {}, target {} (0 is the display's), max "
                     "{}, tolerance {}, {}, flow scale {}%, {}",
                     title, on.mode == settings::FrameGenMode::Adaptive ? "adaptive" : "fixed",
                     on.ratioThousandths, on.targetThousandths, on.maxGenerated,
                     on.toleranceThousandths, on.performance ? "performance" : "balanced",
                     on.flowScalePercent, request->dll.string());
    }
    frameGen_.wrote(std::move(request));
}

void GamePresentation::applyScaling(settings::ScreenSize screen, bool resize) {
    const library::Game* game = running_.get();
    settings::ScreenSize size = screen;
    Upscale upscale{.sharpness = gamescopeDefaultSharpness};
    if (game != nullptr) {
        const settings::ScalingSettings resolved =
            settings::resolveScaling(gameSettings_.layers(), library::layerKeysOf(*game));
        size = resolved.renderSize(screen);
        upscale = Upscale{.filter = resolved.filter, .sharpness = resolved.gamescopeSharpness()};
    }
    const std::string title = game != nullptr ? game->title : std::string{"no game"};
    if (resize && size_.differs(size)) {
        output_->resize(size);
        size_.wrote(size);
        lucent::info("scaling", "{}: games see a {} x {} screen", title, size.width, size.height);
    }
    if (upscale_.differs(upscale)) {
        output_->upscale(upscale);
        upscale_.wrote(upscale);
        lucent::info("scaling", "{}: upscaled with {}, Gamescope sharpness {}", title,
                     settings::scalingFilterChoices[static_cast<std::size_t>(upscale.filter)].key,
                     upscale.sharpness);
    }
}

std::string GamePresentation::noted(std::string text) const {
    if (!active()) {
        text += " · applies in openSU's Gamescope session";
    }
    return text;
}

PresentationSummary GamePresentation::frameGenSummary(const settings::SettingLookup& applies,
                                                      const std::string& where) const {
    return PresentationSummary{
        .id = "framegen.summary",
        .line = settings::frameGenSummary(settings::resolveFrameGen(applies, where)),
        .note = noted(dll_ ? "Lossless Scaling's shaders, from your Steam install"
                           : "Needs Lossless Scaling, installed through Steam")};
}

PresentationSummary GamePresentation::scalingSummary(const settings::SettingLookup& applies,
                                                     const std::string& where) const {
    return PresentationSummary{
        .id = "scaling.summary",
        .line = settings::scalingSummary(settings::resolveScaling(applies, where), screen_),
        .note = noted("Gamescope draws the game at this size and upscales it to the screen")};
}

} // namespace opensu::app
