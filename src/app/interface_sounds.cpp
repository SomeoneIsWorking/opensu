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
    const std::filesystem::path file = iisu ? chosen.iisuFile : std::filesystem::path{};
    if (loaded_ && owner_ == chosen.interfaceSounds && file_ == file) {
        return;
    }
    loaded_ = true;
    owner_ = chosen.interfaceSounds;
    file_ = file;
    sounds_.loadSet(assetsDir_ / audio::shippedDirectory);
    if (!iisu) {
        return;
    }
    std::string error;
    const std::shared_ptr<const artwork::IisuPack> pack = artwork::IisuPack::open(file, error);
    if (pack == nullptr || !pack->hasSounds()) {
        lucent::warn("audio", "iiSU's sounds: {}; playing openSU's",
                     pack == nullptr ? error : file.filename().string() + " holds no sounds");
        return;
    }
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
                 file.filename().string());
}

} // namespace opensu::app
