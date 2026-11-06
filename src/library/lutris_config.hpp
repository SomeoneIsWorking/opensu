// lutris_config — values in Lutris's YAML files (`runners/<runner>.yml`, `games/<config>.yml`),
// read only. Lutris writes them as plain nested mappings, one `key: value` a line, indented by
// spaces; lists and flow styles are skipped.
#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opensu::library::lutris {

/// The value at `keys`, each one mapping deeper (`{"system", "env", "GAMEID"}`), quotes dropped;
/// empty when the file or the key is not there.
[[nodiscard]] std::string configValue(const std::filesystem::path& file,
                                      std::initializer_list<std::string_view> keys);

/// The keys and values directly under `keys`, in the file's order; nothing when the file or the
/// mapping is not there.
[[nodiscard]] std::vector<std::pair<std::string, std::string>>
configMapping(const std::filesystem::path& file, std::initializer_list<std::string_view> keys);

/// A game's own config, `games/<configpath>.yml`, by `configpath` in `pga.db`.
[[nodiscard]] std::filesystem::path gameConfig(const std::filesystem::path& lutrisData,
                                               std::string_view configpath);

} // namespace opensu::library::lutris
