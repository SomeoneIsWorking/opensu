#include "interface_sounds.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "artwork/iisu_pack.hpp"
#include "lucent/log.h"

namespace opensu::app {

void InterfaceSounds::apply(const settings::Settings& chosen) {
    const bool iisu = chosen.interfaceSounds == settings::AssetOwner::Iisu;
    std::shared_ptr<const artwork::IisuPack> pack = iisu ? iisu_.pack() : nullptr;
    if (loaded_ && owner_ == chosen.interfaceSounds && pack_ == pack) {
        return;
    }
    loaded_ = true;
    owner_ = chosen.interfaceSounds;
    pack_ = pack;
    sounds_.loadSet(assetsDir_ / audio::shippedDirectory);
    if (!iisu) {
        return;
    }
    if (pack == nullptr || !pack->hasSounds()) {
        lucent::warn("audio", "iiSU's sounds: {}; playing openSU's",
                     pack == nullptr ? "no iiSU release in " + iisu_.folder().string()
                                     : pack->file().filename().string() + " holds no sounds");
        return;
    }
    std::string error;
    std::size_t count = 0;
    for (const audio::Effect effect : audio::allEffects) {
        const std::string_view name = audio::iisuFile(effect);
        const std::optional<std::string> bytes = pack->sound(name, error);
        if (!bytes) {
            lucent::warn("audio", "iiSU's {}: {}; playing openSU's", name,
                         error.empty() ? "not in the file" : error);
            continue;
        }
        if (sounds_.load(effect, *bytes, name)) {
            ++count;
        }
    }
    lucent::info("audio", "{} of {} UI sounds are iiSU's, from {}", count, audio::allEffects.size(),
                 pack->file().filename().string());
}

} // namespace opensu::app
