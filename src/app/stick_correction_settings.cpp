#include "stick_correction_settings.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace opensu::app {
namespace {

using gamepad::HallMode;
using gamepad::HallTerms;

constexpr const char* modeId = "sticks.correction";
constexpr const char* stretchId = "sticks.stretch";

/// A Manual term's slider: its id, label, range in percent and the term it sets.
struct Term {
    const char* id;
    const char* label;
    int low;
    int high;
    float HallTerms::* term;
};

constexpr std::array<Term, 5> terms{{
    {"sticks.leftIdle", "Left stick rest (x)", -50, 50, &HallTerms::leftIdleX},
    {"sticks.rightIdle", "Right stick rest (x)", -50, 50, &HallTerms::rightIdleX},
    {"sticks.triggerPull", "Left trigger's pull", -50, 50, &HallTerms::triggerPullX},
    {"sticks.leftDeadzone", "Left deadzone", 0, 50, &HallTerms::leftDeadzone},
    {"sticks.rightDeadzone", "Right deadzone", 0, 50, &HallTerms::rightDeadzone},
}};

constexpr std::array<HallMode, 3> modes{HallMode::Off, HallMode::Default, HallMode::Manual};

int percentOf(float fraction) {
    return static_cast<int>(std::lround(fraction * 100.0f));
}

} // namespace

StickCorrectionSettings::StickCorrectionSettings(Preferences& preferences, gamepad::PadSource& pads,
                                                 bool shown, std::function<void()> changed)
    : preferences_{preferences}, pads_{pads}, shown_{shown}, changed_{std::move(changed)} {
    applySaved();
}

void StickCorrectionSettings::applySaved() {
    if (shown_) {
        pads_.setHallTerms(inForce());
    }
}

std::optional<HallTerms> StickCorrectionSettings::inForce() const {
    const settings::Settings& values = preferences_.values();
    if (!shown_ || values.stickCorrection == HallMode::Off) {
        return std::nullopt;
    }
    return values.stickCorrection == HallMode::Manual ? values.stickTerms : HallTerms{};
}

std::vector<ui::SettingsRow> StickCorrectionSettings::rows() const {
    std::vector<ui::SettingsRow> rows;
    if (!shown_) {
        return rows;
    }
    const settings::Settings& values = preferences_.values();
    rows.push_back(ui::makeHeading("Sticks"));
    rows.push_back(ui::makeChoice(
        modeId, "Stick correction",
        "Takes the left trigger's pull off the left stick, and each stick's resting offset",
        ui::Options{.labels = {"Off", "Default", "Manual"},
                    .chosen = static_cast<std::size_t>(values.stickCorrection)}));
    if (values.stickCorrection != HallMode::Manual) {
        return rows;
    }
    for (const Term& term : terms) {
        rows.push_back(ui::makeSlider(
            term.id, term.label, {},
            ui::SliderRange{percentOf(values.stickTerms.*term.term), term.low, term.high, 1}));
    }
    rows.push_back(ui::makeToggle(stretchId, "Stretch past the deadzone",
                                  "The travel past a deadzone reaches the stick's full range",
                                  values.stickTerms.remapDeadzone));
    return rows;
}

bool StickCorrectionSettings::owns(const std::string& id) {
    return id == modeId || id == stretchId || std::ranges::any_of(terms, [&id](const Term& term) {
               return id == term.id;
           });
}

void StickCorrectionSettings::run(const std::string& id, int step) {
    settings::Settings& values = preferences_.values();
    if (id == modeId) {
        const auto count = static_cast<int>(modes.size());
        const int from = static_cast<int>(values.stickCorrection);
        values.stickCorrection = modes[static_cast<std::size_t>(
            ((from + (step == 0 ? 1 : step)) % count + count) % count)];
    } else if (id == stretchId) {
        values.stickTerms.remapDeadzone = !values.stickTerms.remapDeadzone;
    } else {
        return;
    }
    apply();
}

void StickCorrectionSettings::setLevel(const std::string& id, int level) {
    const auto term = std::ranges::find_if(terms, [&id](const Term& each) {
        return id == each.id;
    });
    if (term == terms.end()) {
        return;
    }
    preferences_.values().stickTerms.*term->term =
        static_cast<float>(std::clamp(level, term->low, term->high)) / 100.0f;
    apply();
}

void StickCorrectionSettings::apply() {
    preferences_.save();
    applySaved();
    changed_();
}

} // namespace opensu::app
