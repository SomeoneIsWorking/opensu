#include "interface_sound_settings.hpp"

#include <memory>

#include "artwork/iisu_pack.hpp"

namespace opensu::app {
namespace {

constexpr const char* interfaceSoundsId = "audio.interfacesounds";

std::string note(const settings::Settings& chosen) {
    if (chosen.interfaceSounds == settings::AssetOwner::OpenSu) {
        return "openSU's own sounds";
    }
    return "iiSU's, from " + chosen.iisuFile.filename().string();
}

/// Whether `file` is an iiSU release that holds the sounds.
bool hasSounds(const std::filesystem::path& file) {
    std::string error;
    const std::shared_ptr<const artwork::IisuPack> pack = artwork::IisuPack::open(file, error);
    return pack != nullptr && pack->hasSounds();
}

} // namespace

ui::SettingsRow InterfaceSoundSettings::row() const {
    const settings::Settings& chosen = preferences_.values();
    ui::Options choices;
    for (std::size_t i = 0; i < settings::allAssetOwners.size(); ++i) {
        choices.labels.emplace_back(settings::label(settings::allAssetOwners[i]));
        if (settings::allAssetOwners[i] == chosen.interfaceSounds) {
            choices.chosen = i;
        }
    }
    return ui::makeChoice(interfaceSoundsId, "Interface sounds", note(chosen), std::move(choices));
}

bool InterfaceSoundSettings::owns(const std::string& id) {
    return id == interfaceSoundsId;
}

void InterfaceSoundSettings::run(const std::string& /*id*/, int /*step*/) {
    const settings::Settings& chosen = preferences_.values();
    if (chosen.interfaceSounds == settings::AssetOwner::Iisu) {
        use(settings::AssetOwner::OpenSu, chosen.iisuFile);
        return;
    }
    if (!chosen.iisuFile.empty() && hasSounds(chosen.iisuFile)) {
        use(settings::AssetOwner::Iisu, chosen.iisuFile);
        return;
    }
    chooser_.choose(IisuFileChooser::Need::Sounds, chosen.iisuFile,
                    [this](const std::filesystem::path& file) {
                        const bool newFile = file != preferences_.values().iisuFile;
                        use(settings::AssetOwner::Iisu, file);
                        if (newFile) {
                            hooks_.fileChanged();
                        }
                    });
}

void InterfaceSoundSettings::use(settings::AssetOwner owner,
                                 const std::filesystem::path& iisuFile) {
    preferences_.values().interfaceSounds = owner;
    preferences_.values().iisuFile = iisuFile;
    preferences_.save();
    hooks_.changed();
}

} // namespace opensu::app
