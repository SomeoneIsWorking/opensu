// android_apps — the Android apps installed beside openSU, which standalone emulators' Android
// launchers are matched against; none off Android.
#pragma once

#include <string>
#include <vector>

namespace opensu::library::roms {

/// Package names of the installed apps with a launcher activity.
[[nodiscard]] std::vector<std::string> installedAndroidApps();

} // namespace opensu::library::roms
