// interface_size_setting — the Interface size row, the same in the Guide menu and Settings ›
// Appearance: the sizes from `settings::minUiScale` to `maxUiScale`, saved in
// `settings::Settings::uiScale`.
#pragma once

#include <string>

#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

/// The row as `id`, on the saved size.
[[nodiscard]] ui::SettingsRow interfaceSizeRow(std::string id, const settings::Settings& values);

/// Steps the saved size `step` sizes and saves it; the caller lays the screen out again.
void stepInterfaceSize(Preferences& preferences, int step);

} // namespace opensu::app
