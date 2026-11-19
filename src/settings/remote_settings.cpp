#include "remote_settings.hpp"

#include <string>

namespace opensu::settings {

void declareRemote(Schema& schema) {
    schema.add(choiceSetting(remoteStallsKey,
                             "Smooth over stalls holds frames as long as the link's recent stalls, "
                             "so a burst after one plays out; it adds that much latency while "
                             "stalls go on",
                             std::string{remotePlaySection}, SettingType::Choice));
}

bool resolveRidesOutStalls(const SettingLayers& layers, const LayerKeys& keys) {
    const std::string kept =
        layers.resolve(keys, remoteStallsKey.key, std::string{remoteStallsKey.defaultKey()}).value;
    return remoteStallChoices[choiceIndexOrDefault(remoteStallsKey, kept, keys.game)].key ==
           "ride-out";
}

} // namespace opensu::settings
