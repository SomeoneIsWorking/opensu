// relative_path — a path another openSU or a share names inside a folder of this one (a save
// slot, a ROM folder, the BIOS folder), checked to stay inside it before anything is read or
// written there.
#pragma once

#include <string_view>

namespace opensu::fileio {

/// Whether `path` stays inside the folder it is taken under: empty, or relative and `/`-separated
/// with no empty, `.` or `..` step, no backslash, drive colon or NUL.
[[nodiscard]] bool staysInside(std::string_view path);

} // namespace opensu::fileio
