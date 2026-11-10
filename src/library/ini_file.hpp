// ini_file — one value of an emulator's or a store tool's INI settings file: `key=value` (or
// `key: value`, as Python's configparser also takes) under `[section]`, both trimmed, the first
// such line winning; and that value set in the file's text, every other byte kept.
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

/// A key under `[section]`.
struct IniKey {
    std::string_view section;
    std::string_view key;
};

/// The value of `key` in the INI `text`.
[[nodiscard]] std::optional<std::string> iniValueIn(std::string_view text, const IniKey& key);

/// `text` with `key` set to `value` as `key=value`: the key's first line replaced, else a line
/// added after its section's last setting, else the section added at the end.
[[nodiscard]] std::string withIniValue(std::string_view text, const IniKey& key,
                                       std::string_view value);

} // namespace opensu::library
