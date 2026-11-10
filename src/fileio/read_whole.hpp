// read_whole — a file's bytes, read whole.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace opensu::fileio {

/// Every byte of `file`; nothing, with `error`, when it cannot be read.
[[nodiscard]] std::optional<std::string> readWhole(const std::filesystem::path& file,
                                                   std::string& error);

} // namespace opensu::fileio
