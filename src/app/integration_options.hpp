// integration_options — the opt-in integrations as the player reads them, in the order the
// first-run chooser and Settings > Integrations list them: what each does and needs, and which
// switch of `settings::IntegrationChoices` it is.
#pragma once

#include <string_view>
#include <vector>

#include "settings/integrations.hpp"

namespace opensu::app {

struct IntegrationEntry {
    std::string_view id;
    std::string_view label;
    /// What it does and what it needs, in one line.
    std::string_view note;
};

/// Steam, Epic, GOG, standalone emulators, Lua plugins: those this build can serve (Steam when its
/// client has a mode, Epic and GOG where their command-line clients run).
[[nodiscard]] std::vector<IntegrationEntry> integrationEntries();

/// The switch of `choices` the entry `id` is, or null for no entry.
[[nodiscard]] bool* switchOf(settings::IntegrationChoices& choices, std::string_view id) noexcept;
[[nodiscard]] bool switchOf(const settings::IntegrationChoices& choices,
                            std::string_view id) noexcept;

} // namespace opensu::app
