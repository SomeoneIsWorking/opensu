#include "pad_calibration_flow.hpp"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "pad_check_screen.hpp"

namespace opensu::app {
namespace {

constexpr std::string_view rowPrefix = "calibrate:";
constexpr std::string_view checkPrefix = "check:";
constexpr const char* cardId = "pad.calibration";
constexpr const char* checkId = "pad.check";

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
                                       Say say, bool hallOffered)
    : navigator_{navigator}, preferences_{preferences}, pads_{pads}, sounds_{sounds},
      say_{std::move(say)}, hallOffered_{hallOffered} {
    applySaved();
}

void PadCalibrationFlow::applySaved() {
    pads_.setCalibrations(preferences_.values().padCalibrations);
}

std::vector<ui::SettingsRow> PadCalibrationFlow::rows(const ControllerStatus& pad) const {
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
    return {std::move(row),
            ui::makeRow(std::string{checkPrefix} + pad.id, ui::RowKind::Action, "Check " + pad.name,
                        "Its sticks and triggers as games get them", "Check")};
}

bool PadCalibrationFlow::owns(const std::string& id) {
    return id.starts_with(rowPrefix) || id.starts_with(checkPrefix);
}

const ControllerStatus* PadCalibrationFlow::padOf(const std::string& id,
                                                  const std::vector<ControllerStatus>& pads) {
    const auto found = std::ranges::find_if(pads, [&id](const ControllerStatus& pad) {
        return id == std::string{rowPrefix} + pad.id || id == std::string{checkPrefix} + pad.id;
    });
    return found == pads.end() ? nullptr : &*found;
}

void PadCalibrationFlow::run(const std::string& id, const std::vector<ControllerStatus>& pads) {
    const ControllerStatus* pad = padOf(id, pads);
    if (pad == nullptr || measuring()) {
        return;
    }
    Pad which{pad->id, pad->name, gamepad::calibrationKey(pad->name, pad->uniq)};
    if (id.starts_with(checkPrefix)) {
        check(which, "How games get it now", {});
        return;
    }
    measure(std::move(which));
}

void PadCalibrationFlow::measure(Pad pad) {
    measured_ = Measured{.pad = std::move(pad),
                         .step = hallOffered_ ? Step::Choose : Step::Still,
                         .hall = false,
                         .started = now_,
                         .stepAt = now_,
                         .capture = {}};
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<ui::ProgressScreen>(
        cardId,
        [this] {
            return measured_ ? card() : ui::ProgressCard{};
        },
        [this] {
            stop("Calibration cancelled: nothing changed", false);
        }));
}

ui::ProgressCard PadCalibrationFlow::card() {
    const Measured& now = *measured_;
    ui::ProgressCard shown{.title = "Calibrating " + now.pad.name,
                           .line = {},
                           .fraction = {},
                           .leave = "Cancel",
                           .cover = {},
                           .choices = {}};
    const auto done = [this, &now](Clock::duration whole) {
        return share(now_ - now.stepAt, whole);
    };
    switch (now.step) {
    case Step::Choose:
        shown.line =
            "Also measure how far the left trigger pulls the left stick, so the hall-effect "
            "correction takes off exactly that?";
        shown.choices = {ui::ProgressChoice{.id = "calibrate.hall",
                                            .label = "With hall-effect correction",
                                            .pressed =
                                                [this] {
                                                    begin(true);
                                                }},
                         ui::ProgressChoice{.id = "calibrate.plain",
                                            .label = "Sticks and triggers only",
                                            .pressed = [this] {
                                                begin(false);
                                            }}};
        break;
    case Step::Still:
        shown.line = "Let go of both sticks and both triggers";
        shown.fraction = done(stillFor);
        break;
    case Step::Sweep:
        shown.line =
            "Turn both sticks round their edges, and pull both triggers all the way, until "
            "the bar is full";
        shown.fraction = done(sweepFor);
        break;
    case Step::Pull:
        shown.line =
            "Hold the left trigger all the way down and turn the left stick round its edge, "
            "until the bar is full";
        shown.fraction = done(pullFor);
        break;
    }
    return shown;
}

void PadCalibrationFlow::begin(bool hall) {
    if (!measured_ || measured_->step != Step::Choose) {
        return;
    }
    measured_->hall = hall;
    measured_->step = Step::Still;
    measured_->stepAt = now_;
}

void PadCalibrationFlow::reset(const std::string& id, const std::vector<ControllerStatus>& pads) {
    const ControllerStatus* pad = padOf(id, pads);
    if (pad == nullptr || preferences_.values().padCalibrations.erase(
                              gamepad::calibrationKey(pad->name, pad->uniq)) == 0) {
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
    const auto found = axes.find(measured_->pad.id);
    if (found == axes.end()) {
        stop(measured_->pad.name + " left before it was calibrated", true);
        return;
    }
    switch (measured_->step) {
    case Step::Choose:
        return;
    case Step::Still:
        measured_->capture.still(found->second.raw);
        if (now - measured_->stepAt >= stillFor) {
            measured_->step = Step::Sweep;
            measured_->stepAt = now;
        }
        return;
    case Step::Sweep:
        measured_->capture.sweep(found->second.raw);
        if (now - measured_->stepAt < sweepFor) {
            return;
        }
        if (measured_->hall) {
            measured_->step = Step::Pull;
            measured_->stepAt = now;
            return;
        }
        finish();
        return;
    case Step::Pull:
        measured_->capture.pull(found->second.raw);
        if (now - measured_->stepAt >= pullFor) {
            finish();
        }
        return;
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
    gamepad::PadCalibrations& saved = preferences_.values().padCalibrations;
    std::optional<gamepad::PadCalibration> previous;
    if (const auto was = saved.find(measured_->pad.key); was != saved.end()) {
        previous = was->second;
    }
    saved[measured_->pad.key] = result;
    preferences_.save();
    applySaved();
    const Pad pad = measured_->pad;
    stop({}, false);
    check(pad,
          missed.empty()
              ? "Calibrated"
              : "Calibrated; not reached far enough, so left as it reports: " + joined(missed),
          [this, pad, previous] {
              closeCheck();
              gamepad::PadCalibrations& kept = preferences_.values().padCalibrations;
              if (previous) {
                  kept[pad.key] = *previous;
              } else {
                  kept.erase(pad.key);
              }
              preferences_.save();
              applySaved();
              say_(pad.name + (previous ? " is back to its last calibration"
                                        : " reads as it reports again"),
                   false);
          });
}

void PadCalibrationFlow::check(const Pad& pad, const std::string& line,
                               std::function<void()> undo) {
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<PadCheckScreen>(
        checkId, pad.name, line,
        PadCheckScreen::Hooks{.axes = [this, id = pad.id]() -> std::optional<gamepad::PadAxes> {
                                  const std::map<std::string, gamepad::PadAxes> axes = pads_.axes();
                                  const auto found = axes.find(id);
                                  if (found == axes.end()) {
                                      return std::nullopt;
                                  }
                                  return found->second;
                              },
                              .keep =
                                  [this] {
                                      closeCheck();
                                  },
                              .again =
                                  [this, pad] {
                                      closeCheck();
                                      measure(pad);
                                  },
                              .undo = std::move(undo)}));
}

void PadCalibrationFlow::closeCheck() {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto shown = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return screen->id() == checkId && !screen->closing();
    });
    if (shown != open.end()) {
        // input-sound.md 3.4 Close: a panel goes away.
        sounds_.play(audio::Effect::Close);
        navigator_.closeFrom(**shown);
    }
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
    if (!why.empty()) {
        say_(why, error);
    }
}

} // namespace opensu::app
