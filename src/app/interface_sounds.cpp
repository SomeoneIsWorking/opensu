#include "interface_sounds.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "artwork/iisu_pack.hpp"
#include "lucent/log.h"

namespace opensu::app {

void InterfaceSounds::loadTheme(const theme::InterfaceTheme& theme) {
    std::string error;
    std::size_t count = 0;
    for (const audio::Effect effect : audio::allEffects) {
        const std::string_view name = audio::shippedFile(effect);
        const std::optional<std::filesystem::path> file = theme::soundFile(theme, name, error);
        if (!error.empty()) {
            lucent::warn("audio", "{}'s {} {}; playing {}'s", theme.name, name, error,
                         settings::label(theme.sounds.set));
        }
        if (file && sounds_.load(effect, *file)) {
            ++count;
        }
    }
    lucent::info("audio", "{} of {} UI sounds are {}'s, from {}", count, audio::allEffects.size(),
                 theme.name, theme.sounds.folder.string());
}

void InterfaceSounds::apply(const settings::Settings& chosen,
                            const std::shared_ptr<const theme::InterfaceTheme>& theme) {
    const settings::SoundSet set = chosen.interfaceSounds.value_or(theme->sounds.set);
    const bool iisu = set == settings::SoundSet::Iisu;
    std::shared_ptr<const artwork::IisuPack> pack = iisu ? iisu_.pack() : nullptr;
    std::shared_ptr<const theme::InterfaceTheme> files =
        !chosen.interfaceSounds && !theme->sounds.folder.empty() ? theme : nullptr;
    if (loaded_ && chosen_ == set && pack_ == pack && theme_ == files) {
        return;
    }
    loaded_ = true;
    chosen_ = set;
    pack_ = pack;
    theme_ = files;
    const settings::SoundSet shipped = settings::shippedSet(set);
    sounds_.loadSet(assetsDir_ / audio::shippedDirectory, settings::key(shipped));
    if (files != nullptr) {
        loadTheme(*files);
        return;
    }
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
