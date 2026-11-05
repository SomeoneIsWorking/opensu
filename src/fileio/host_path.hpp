// host_path — the file a path written as text names on this system: UTF-8 as Windows paths are kept
// on Windows, and elsewhere a path a Windows program wrote, with Wine's `Z:` drive (the host's
// root) dropped and its backslashes turned around.
#pragma once

#include <filesystem>
#include <string>

namespace opensu::fileio {

[[nodiscard]] std::filesystem::path pathOnHost(const std::string& text);

} // namespace opensu::fileio
