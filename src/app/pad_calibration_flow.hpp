// pad_calibration_flow — Devices › Controllers' Calibrate row for each pad: a card measures the pad
// (`gamepad::CalibrationCapture`) in timed steps, its rests while it is left alone and then its
// ends while the sticks go round their edges and the triggers are pulled all the way; where the
// hall-effect correction applies (a ROG Ally) the card first asks whether to measure for it too,
// and if so ends with the left stick's circle again while the left trigger is held down, which
// measures the trigger's pull. It saves the result in `settings::Settings::padCalibrations` by the
// pad's `calibrationKey` and hands every saved calibration to the pads, as it does at start. Back
// on the card measures nothing; X on a calibrated pad's row (Reset) forgets its calibration.
#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "controller_roster.hpp"
#include "gamepad/pad_calibration.hpp"
#include "gamepad/pad_source.hpp"
#include "preferences.hpp"
#include "ui/progress_screen.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class PadCalibrationFlow {
  public:
    using Clock = std::chrono::steady_clock;
    /// Says how it went: `error` marks a failure.
    using Say = std::function<void(const std::string& text, bool error)>;

    /// How long the pad is left alone, then swept, then swept with the left trigger held.
    static constexpr std::chrono::seconds stillFor{2};
    static constexpr std::chrono::seconds sweepFor{10};
    static constexpr std::chrono::seconds pullFor{8};

    /// Hands the saved calibrations to `pads`; `hallOffered` asks whether to measure the
    /// hall-effect correction's trigger pull as well.
    PadCalibrationFlow(gpu2d::ui::Navigator& navigator, Preferences& preferences,
                       gamepad::PadSource& pads, audio::SdlSoundPlayer& sounds, Say say,
                       bool hallOffered);

    /// The row for `pad`.
    [[nodiscard]] ui::SettingsRow row(const ControllerStatus& pad) const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Starts measuring the pad of the row `id`.
    void run(const std::string& id, const std::vector<ControllerStatus>& pads);
    /// Forgets the calibration of the row `id`'s pad.
    void reset(const std::string& id, const std::vector<ControllerStatus>& pads);
    /// Samples the pad being measured and moves through the steps. Main loop, every frame.
    void service(Clock::time_point now);
    [[nodiscard]] bool measuring() const noexcept {
        return measured_.has_value();
    }

  private:
    enum class Step : std::uint8_t { Choose, Still, Sweep, Pull };
    struct Measured {
        std::string padId;
        std::string name;
        std::string key;
        Step step{Step::Still};
        bool hall{false};
        Clock::time_point started;
        Clock::time_point stepAt;
        gamepad::CalibrationCapture capture;
    };

    [[nodiscard]] static const ControllerStatus* padOf(const std::string& id,
                                                       const std::vector<ControllerStatus>& pads);
    /// The card now; its choices act on the flow.
    [[nodiscard]] ui::ProgressCard card();
    void begin(bool hall);
    void finish();
    void stop(const std::string& why, bool error);
    void applySaved();

    gpu2d::ui::Navigator& navigator_;
    Preferences& preferences_;
    gamepad::PadSource& pads_;
    audio::SdlSoundPlayer& sounds_;
    Say say_;
    bool hallOffered_;
    std::optional<Measured> measured_;
    Clock::time_point now_;
};

} // namespace opensu::app
