// stick_correction_settings — Settings › Controls › Sticks on a ROG Ally: the hall-effect stick
// correction (`gamepad::HallTerms`), Off, Default or Manual with the player's terms, saved in
// `settings::Settings::stickCorrection` and `stickTerms` and set on the pads, as the saved one is
// at start. Elsewhere it has no rows and sets nothing.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "gamepad/pad_source.hpp"
#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class StickCorrectionSettings {
  public:
    /// On a ROG Ally (`shown`) sets the saved correction on `pads`; `changed` runs when the rows
    /// read differently.
    StickCorrectionSettings(Preferences& preferences, gamepad::PadSource& pads, bool shown,
                            std::function<void()> changed);

    /// The Controls page's rows: a heading, the mode and, for Manual, the terms.
    [[nodiscard]] std::vector<ui::SettingsRow> rows() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Steps the mode, or flips the deadzone's stretch.
    void run(const std::string& id, int step);
    /// Sets a term's slider, in percent of a stick's half travel.
    void setLevel(const std::string& id, int level);
    /// Sets the saved correction on the pads, after the settings changed elsewhere.
    void applySaved();

  private:
    /// The terms in force for the saved mode; none when Off or not shown.
    [[nodiscard]] std::optional<gamepad::HallTerms> inForce() const;
    void apply();

    Preferences& preferences_;
    gamepad::PadSource& pads_;
    bool shown_;
    std::function<void()> changed_;
};

} // namespace opensu::app
