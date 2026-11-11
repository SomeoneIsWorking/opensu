// atomic_write — replacing a file whole, so a reader sees the old file or the new one.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace opensu::fileio {

/// Writes `bytes` to `<file>.part` beside `file` and renames it over `file`, making the directories
/// it needs. False with `error` when it cannot; `file` is then as it was and no part is left.
[[nodiscard]] bool writeWhole(const std::filesystem::path& file, std::string_view bytes,
                              std::string& error);

} // namespace opensu::fileio
