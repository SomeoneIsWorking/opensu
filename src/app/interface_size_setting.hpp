// interface_size_setting — the Interface size slider, the same in the Guide menu and Settings ›
// Appearance: from `settings::minUiScale` to `maxUiScale` in `uiScaleStep`s, saved in
// `settings::Settings::uiScale`.
#pragma once

#include <string>

#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

/// The row as `id`, on the saved size.
[[nodiscard]] ui::SettingsRow interfaceSizeRow(std::string id, const settings::Settings& values);

/// Saves the size the slider was moved to; the caller lays the screen out again.
void setInterfaceSize(Preferences& preferences, int percent);

} // namespace opensu::app
