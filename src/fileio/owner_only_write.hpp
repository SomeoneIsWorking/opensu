// owner_only_write — a file only its owner may read, for sign-ins and keys.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace opensu::fileio {

/// Writes `bytes` to `file` readable by its owner only (0600), replacing it atomically, in a
/// directory made owner-only (0700). False with `error` when it cannot; `file` is then as it was.
[[nodiscard]] bool writeOwnerOnly(const std::filesystem::path& file, std::string_view bytes,
                                  std::string& error);

} // namespace opensu::fileio
