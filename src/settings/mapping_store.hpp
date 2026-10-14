// mapping_store — the button mappings and the Steam Input switch (`input::MappingScopes`) in
// `mappings.json` under the config directory, beside `settings.json` and kept apart from it because
// it grows per game. Written whole and atomically; a damaged file or entry is reported and skipped.
#pragma once

#include <filesystem>
#include <string>

#include "input/mapping_scopes.hpp"

namespace opensu::settings {

/// The file's name in the config directory.
inline constexpr std::string_view mappingsFile = "mappings.json";

class MappingStore {
  public:
    explicit MappingStore(std::filesystem::path file) : file_{std::move(file)} {
    }

    /// The saved mappings. A missing file gives none; an unreadable or unparsable file, or a
    /// damaged mapping or entry, is reported and left out.
    [[nodiscard]] input::MappingScopes load() const;

    /// Replaces the file whole. False with `error` when it cannot; the old file is then kept.
    [[nodiscard]] bool save(const input::MappingScopes& scopes, std::string& error) const;

    [[nodiscard]] const std::filesystem::path& file() const noexcept {
        return file_;
    }

  private:
    std::filesystem::path file_;
};

} // namespace opensu::settings
