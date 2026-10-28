#include "interface_sound_settings.hpp"

#include <memory>

namespace opensu::app {
namespace {

constexpr const char* interfaceSoundsId = "audio.interfacesounds";

} // namespace

std::shared_ptr<const artwork::IisuPack> InterfaceSoundSettings::iisuSounds() const {
    std::shared_ptr<const artwork::IisuPack> pack = iisu_.pack();
    return pack != nullptr && pack->hasSounds() ? pack : nullptr;
}

ui::SettingsRow InterfaceSoundSettings::row() const {
    const std::shared_ptr<const artwork::IisuPack> pack = iisuSounds();
    const bool iisu =
        pack != nullptr && preferences_.values().interfaceSounds == settings::AssetOwner::Iisu;
    ui::Options choices;
    for (const settings::AssetOwner owner : settings::allAssetOwners) {
        if (owner == settings::AssetOwner::Iisu && pack == nullptr) {
            continue;
        }
        if (owner == (iisu ? settings::AssetOwner::Iisu : settings::AssetOwner::OpenSu)) {
            choices.chosen = choices.labels.size();
        }
        choices.labels.emplace_back(settings::label(owner));
    }
    const std::string note =
        iisu ? "iiSU's, from " + pack->file().filename().string() : "openSU's own sounds";
    return ui::makeChoice(interfaceSoundsId, "Interface sounds", note, std::move(choices));
}

bool InterfaceSoundSettings::owns(const std::string& id) {
    return id == interfaceSoundsId;
}

void InterfaceSoundSettings::run(const std::string& /*id*/, int /*step*/) {
    if (iisuSounds() == nullptr) {
        return;
    }
    settings::AssetOwner& owner = preferences_.values().interfaceSounds;
    owner = owner == settings::AssetOwner::Iisu ? settings::AssetOwner::OpenSu
                                                : settings::AssetOwner::Iisu;
    preferences_.save();
    changed_();
}

} // namespace opensu::app
