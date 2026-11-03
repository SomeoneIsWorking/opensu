#include "interface_sounds.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "artwork/iisu_pack.hpp"
#include "lucent/log.h"

namespace opensu::app {

void InterfaceSounds::apply(const settings::Settings& chosen) {
    const bool iisu = chosen.interfaceSounds == settings::SoundSet::Iisu;
    std::shared_ptr<const artwork::IisuPack> pack = iisu ? iisu_.pack() : nullptr;
    if (loaded_ && chosen_ == chosen.interfaceSounds && pack_ == pack) {
        return;
    }
    loaded_ = true;
    chosen_ = chosen.interfaceSounds;
    pack_ = pack;
    const settings::SoundSet shipped = settings::shippedSet(chosen.interfaceSounds);
    sounds_.loadSet(assetsDir_ / audio::shippedDirectory, settings::key(shipped));
    if (!iisu) {
        return;
    }
    if (pack == nullptr || !pack->hasSounds()) {
        lucent::warn("audio", "iiSU's sounds: {}; playing {}",
                     pack == nullptr ? "no iiSU release in " + iisu_.folder().string()
                                     : pack->file().filename().string() + " holds no sounds",
                     settings::label(shipped));
        return;
    }
    std::string error;
    std::size_t count = 0;
    for (const audio::Effect effect : audio::allEffects) {
        const std::string_view name = audio::iisuFile(effect);
        const std::optional<std::string> bytes = pack->sound(name, error);
        if (!bytes) {
            lucent::warn("audio", "iiSU's {}: {}; playing {}'s", name,
                         error.empty() ? "not in the file" : error, settings::label(shipped));
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
