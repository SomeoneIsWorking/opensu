#include "interface_sound_settings.hpp"

#include <memory>
#include <vector>

namespace opensu::app {
namespace {

constexpr const char* interfaceSoundsId = "audio.interfacesounds";

} // namespace

std::shared_ptr<const artwork::IisuPack> InterfaceSoundSettings::iisuSounds() const {
    std::shared_ptr<const artwork::IisuPack> pack = iisu_.pack();
    return pack != nullptr && pack->hasSounds() ? pack : nullptr;
}

std::vector<settings::SoundSet> InterfaceSoundSettings::offered() const {
    const bool iisu = iisuSounds() != nullptr;
    std::vector<settings::SoundSet> sets;
    for (const settings::SoundSet set : settings::allSoundSets) {
        if (set != settings::SoundSet::Iisu || iisu) {
            sets.push_back(set);
        }
    }
    return sets;
}

settings::SoundSet InterfaceSoundSettings::playing() const {
    const settings::SoundSet chosen = preferences_.values().interfaceSounds;
    return iisuSounds() == nullptr ? settings::shippedSet(chosen) : chosen;
}

ui::SettingsRow InterfaceSoundSettings::row() const {
    const settings::SoundSet now = playing();
    ui::Options choices;
    for (const settings::SoundSet set : offered()) {
        if (set == now) {
            choices.chosen = choices.labels.size();
        }
        choices.labels.emplace_back(settings::label(set));
    }
    const std::string note = now == settings::SoundSet::Iisu
                                 ? "iiSU's, from " + iisuSounds()->file().filename().string()
                                 : "openSU's own sounds";
    return ui::makeChoice(interfaceSoundsId, "Interface sounds", note, std::move(choices));
}

bool InterfaceSoundSettings::owns(const std::string& id) {
    return id == interfaceSoundsId;
}

void InterfaceSoundSettings::run(const std::string& /*id*/, int step) {
    preferences_.values().interfaceSounds = ui::stepped(offered(), playing(), step);
    preferences_.save();
    changed_();
}

} // namespace opensu::app
