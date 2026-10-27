// binary — Valve's binary KeyValues, the shape Steam writes `shortcuts.vdf` in: a type byte, a
// NUL-ended key, then a NUL-ended string, a little-endian 32-bit integer or a nested block that
// ends with 0x08. Integers become decimal strings, so the result reads like a text document.
#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "node.hpp"

namespace opensu::vdf {

/// Parses a binary document; nothing when it is malformed.
[[nodiscard]] std::optional<Node> parseBinary(std::string_view bytes);

/// Parses a binary file, returning nothing when it cannot be read or is malformed.
[[nodiscard]] std::optional<Node> parseBinaryFile(const std::filesystem::path& path);

} // namespace opensu::vdf
