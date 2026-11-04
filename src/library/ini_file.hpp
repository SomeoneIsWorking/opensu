// ini_file — one value of an emulator's or a store tool's INI settings file: `key=value` (or
// `key: value`, as Python's configparser also takes) under `[section]`, both trimmed, the first
// such line winning.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace opensu::library {

/// The value of `key` in `[section]` of `file`; nothing when the file, the section or the key is
/// not there.
[[nodiscard]] std::optional<std::string> iniValue(const std::filesystem::path& file,
                                                  std::string_view section, std::string_view key);

} // namespace opensu::library
