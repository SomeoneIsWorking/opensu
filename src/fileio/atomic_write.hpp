// atomic_write — replacing a file whole, so a reader sees the old file or the new one.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace iideck::fileio {

/// Writes `bytes` beside `file` and renames them over it, making the directories it needs. False
/// with `error` when it cannot; `file` is then as it was.
[[nodiscard]] bool writeWhole(const std::filesystem::path& file, std::string_view bytes,
                              std::string& error);

} // namespace iideck::fileio
