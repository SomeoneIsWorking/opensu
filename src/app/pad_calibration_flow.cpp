#include "pad_calibration_flow.hpp"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "ui/progress_screen.hpp"

namespace opensu::app {
namespace {

constexpr std::string_view rowPrefix = "calibrate:";
constexpr const char* cardId = "pad.calibration";

std::string joined(const std::vector<std::string>& parts) {
    std::string text;
    for (const std::string& part : parts) {
        text += (text.empty() ? "" : ", ") + part;
    }
    return text;
}

float share(PadCalibrationFlow::Clock::duration done, PadCalibrationFlow::Clock::duration whole) {
    return std::clamp(std::chrono::duration<float>(done) / std::chrono::duration<float>(whole),
                      0.0f, 1.0f);
}

} // namespace

PadCalibrationFlow::PadCalibrationFlow(gpu2d::ui::Navigator& navigator, Preferences& preferences,
                                       gamepad::PadSource& pads, audio::SdlSoundPlayer& sounds,
                                       Say say)
    : navigator_{navigator}, preferences_{preferences}, pads_{pads}, sounds_{sounds},
      say_{std::move(say)} {
    applySaved();
}

void PadCalibrationFlow::applySaved() {
    pads_.setCalibrations(preferences_.values().padCalibrations);
}

ui::SettingsRow PadCalibrationFlow::row(const ControllerStatus& pad) const {
    const bool calibrated =
        preferences_.values().padCalibrations.contains(gamepad::calibrationKey(pad.name, pad.uniq));
    ui::SettingsRow row = ui::makeRow(
        std::string{rowPrefix} + pad.id, ui::RowKind::Action, "Calibrate " + pad.name,
        calibrated ? "Its sticks' centres and ends and its triggers' ends as measured"
                   : "Centres its sticks and makes their ends and the triggers' reach full travel",
        calibrated ? "Again" : "Calibrate");
    if (calibrated) {
        row.secondary = "Reset";
    }
    return row;
}

bool PadCalibrationFlow::owns(const std::string& id) {
    return id.starts_with(rowPrefix);
}

const ControllerStatus* PadCalibrationFlow::padOf(const std::string& id,
                                                  const std::vector<ControllerStatus>& pads) {
    const auto found = std::ranges::find_if(pads, [&id](const ControllerStatus& pad) {
        return id == std::string{rowPrefix} + pad.id;
    });
    return found == pads.end() ? nullptr : &*found;
}

void PadCalibrationFlow::run(const std::string& id, const std::vector<ControllerStatus>& pads) {
    const ControllerStatus* pad = padOf(id, pads);
    if (pad == nullptr || measuring()) {
        return;
    }
    measured_ = Measured{.padId = pad->id,
                         .name = pad->name,
                         .key = gamepad::calibrationKey(pad->name, pad->uniq),
                         .step = Step::Still,
                         .started = now_,
                         .stepAt = now_,
                         .capture = {}};
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<ui::ProgressScreen>(
        cardId,
        [this] {
            if (!measured_) {
                return ui::ProgressCard{};
            }
            const bool still = measured_->step == Step::Still;
            return ui::ProgressCard{
                .title = "Calibrating " + measured_->name,
                .line = still ? "Let go of both sticks and both triggers"
                              : "Turn both sticks round their edges, and pull both triggers all "
                                "the way, until the bar is full",
                .fraction = share(now_ - measured_->stepAt, still ? Clock::duration{stillFor}
                                                                  : Clock::duration{sweepFor}),
                .leave = "Cancel",
                .cover = {}};
        },
        [this] {
            stop("Calibration cancelled: nothing changed", false);
        }));
}

void PadCalibrationFlow::reset(const std::string& id, const std::vector<ControllerStatus>& pads) {
    const ControllerStatus* pad = padOf(id, pads);
    if (pad == nullptr ||
        preferences_.values().padCalibrations.erase(gamepad::calibrationKey(pad->name, pad->uniq)) ==
            0) {
        return;
    }
    preferences_.save();
    applySaved();
    say_(pad->name + " reads as it reports again", false);
}

void PadCalibrationFlow::service(Clock::time_point now) {
    now_ = now;
    if (!measured_) {
        return;
    }
    const std::map<std::string, gamepad::PadAxes> axes = pads_.axes();
    const auto found = axes.find(measured_->padId);
    if (found == axes.end()) {
        stop(measured_->name + " left before it was calibrated", true);
        return;
    }
    if (measured_->step == Step::Still) {
        measured_->capture.still(found->second.raw);
        if (now - measured_->stepAt >= stillFor) {
            measured_->step = Step::Sweep;
            measured_->stepAt = now;
        }
        return;
    }
    measured_->capture.sweep(found->second.raw);
    if (now - measured_->stepAt >= sweepFor) {
        finish();
    }
}

void PadCalibrationFlow::finish() {
    const gamepad::PadCalibration result = measured_->capture.result();
    const std::vector<std::string> missed = measured_->capture.missed();
    if (result.axes.empty()) {
        stop("Nothing was measured: turn both sticks round their edges and pull both triggers all "
             "the way while the bar fills",
             true);
        return;
    }
    preferences_.values().padCalibrations[measured_->key] = result;
    preferences_.save();
    applySaved();
    stop(measured_->name + " calibrated" +
             (missed.empty() ? "" : "; not reached far enough, so left as it reports: " +
                                        joined(missed)),
         false);
}

void PadCalibrationFlow::stop(const std::string& why, bool error) {
    measured_.reset();
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto card = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return screen->id() == cardId && !screen->closing();
    });
    if (card != open.end()) {
        navigator_.closeFrom(**card);
    }
    say_(why, error);
}

} // namespace opensu::app
