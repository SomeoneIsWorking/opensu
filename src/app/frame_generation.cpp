#include "frame_generation.hpp"

#include "library/emulator_choice.hpp"
#include "lucent/log.h"

namespace opensu::app {

FrameGeneration::FrameGeneration(const GameSettings& gameSettings,
                                 std::unique_ptr<FrameGenOutput> output,
                                 std::function<std::optional<std::filesystem::path>()> findDll)
    : gameSettings_{gameSettings}, output_{std::move(output)}, findDll_{std::move(findDll)} {
    if (supported()) {
        dll_ = findDll_();
    }
}

void FrameGeneration::findDll() {
    if (!supported()) {
        return;
    }
    dll_ = findDll_();
    if (output_) {
        apply();
    }
}

void FrameGeneration::follow(const library::Game* inFront) {
    if (!output_) {
        return;
    }
    const bool same = inFront == nullptr ? !front_ : front_ && front_->id == inFront->id;
    if (same && written_) {
        return;
    }
    if (inFront == nullptr) {
        front_.reset();
    } else {
        front_ = *inFront;
        dll_ = findDll_();
    }
    apply();
}

void FrameGeneration::changed() {
    if (output_) {
        apply();
    }
}

std::optional<FrameGenRequest> FrameGeneration::wanted() const {
    if (!front_ || !dll_) {
        return std::nullopt;
    }
    const settings::FrameGenSettings resolved =
        settings::resolveFrameGen(gameSettings_.layers(), library::layerKeysOf(*front_));
    if (resolved.mode == settings::FrameGenMode::Off) {
        return std::nullopt;
    }
    return FrameGenRequest{resolved, *dll_};
}

void FrameGeneration::apply() {
    const std::optional<FrameGenRequest> request = wanted();
    if (written_ && *written_ == request) {
        return;
    }
    written_ = request;
    const std::string title = front_ ? front_->title : std::string{};
    if (!request) {
        output_->off();
        lucent::info("framegen", "frame generation off{}",
                     front_ ? " for " + title + (dll_ ? "" : ": no Lossless.dll")
                            : std::string{": no game in front"});
        return;
    }
    output_->generate(*request);
    const settings::FrameGenSettings& on = request->settings;
    lucent::info("framegen",
                 "frame generation for {}: {}, ratio {}, target {} (0 is the display's), max {}, "
                 "tolerance {}, {}, flow scale {}%, {}",
                 title, on.mode == settings::FrameGenMode::Adaptive ? "adaptive" : "fixed",
                 on.ratioThousandths, on.targetThousandths, on.maxGenerated,
                 on.toleranceThousandths, on.performance ? "performance" : "balanced",
                 on.flowScalePercent, request->dll.string());
}

} // namespace opensu::app
