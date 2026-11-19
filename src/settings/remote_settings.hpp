// remote_settings — how a game played from another openSU is shown here: `remote.stalls`, whether
// the guest shows the newest frame (lowest latency) or holds frames to ride out the link's stalls
// (`stream::PlayoutDelay`), smoother at the latency of the stalls it waits for.
#pragma once

#include <array>

#include "setting_choices.hpp"
#include "setting_layers.hpp"
#include "setting_schema.hpp"

namespace opensu::settings {

inline constexpr std::array<ChoiceText, 2> remoteStallChoices{{
    {"newest", "Lowest latency"},
    {"ride-out", "Smooth over stalls"},
}};
inline constexpr ChoiceKey remoteStallsKey{"remote.stalls", "Wi-Fi stalls", remoteStallChoices, 0};

/// Declares the remote play settings in `schema`.
void declareRemote(Schema& schema);
/// Whether `keys`' remote game holds frames to ride out the link's stalls.
[[nodiscard]] bool resolveRidesOutStalls(const SettingLayers& layers, const LayerKeys& keys);

} // namespace opensu::settings
