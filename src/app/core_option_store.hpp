// core_option_store — the options each built-in core defines, kept from the last time a game ran
// on it (`<cache>/core-options/<core id>.json`), so a core's options can be set at its layer and a
// game's while nothing runs. The running core's own list always wins and is kept again.
#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "settings/core_option_settings.hpp"

namespace opensu::app {

class CoreOptionStore {
  public:
    explicit CoreOptionStore(std::filesystem::path folder) : folder_{std::move(folder)} {
    }

    /// The options `core` (a catalog id) defined when last run; none when it never ran here or
    /// the file is damaged (reported).
    [[nodiscard]] std::vector<settings::CoreOptionDefinition> known(std::string_view core) const;
    /// Keeps `options` as `core`'s; a failure to write is reported and changes nothing else.
    void remember(std::string_view core,
                  const std::vector<settings::CoreOptionDefinition>& options) const;

  private:
    [[nodiscard]] std::filesystem::path fileOf(std::string_view core) const;

    std::filesystem::path folder_;
};

} // namespace opensu::app
