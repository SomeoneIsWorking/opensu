#include "interface_sound_settings.hpp"

#include <memory>
#include <vector>

namespace opensu::app {
namespace {

constexpr const char* interfaceSoundsId = "audio.interfacesounds";
constexpr const char* menuMusicId = "audio.menumusic";

} // namespace

std::shared_ptr<const artwork::IisuPack> InterfaceSoundSettings::iisuSounds() const {
    std::shared_ptr<const artwork::IisuPack> pack = iisu_.pack();
    return pack != nullptr && pack->hasSounds() ? pack : nullptr;
}

std::vector<std::optional<settings::SoundSet>> InterfaceSoundSettings::offered() const {
    const bool iisu = iisuSounds() != nullptr;
    std::vector<std::optional<settings::SoundSet>> sets{std::nullopt};
    for (const settings::SoundSet set : settings::allSoundSets) {
        if (set != settings::SoundSet::Iisu || iisu) {
            sets.emplace_back(set);
        }
    }
    return sets;
}

std::optional<settings::SoundSet> InterfaceSoundSettings::playing() const {
    const std::optional<settings::SoundSet> chosen = preferences_.values().interfaceSounds;
    return chosen && iisuSounds() == nullptr ? settings::shippedSet(*chosen) : chosen;
}

ui::SettingsRow InterfaceSoundSettings::row() const {
    const std::optional<settings::SoundSet> now = playing();
    ui::Options choices;
    for (const std::optional<settings::SoundSet>& set : offered()) {
        if (set == now) {
            choices.chosen = choices.labels.size();
        }
        choices.labels.emplace_back(set ? settings::label(*set) : "Theme's");
    }
    std::string note = "openSU's own sounds";
    if (!now) {
        note = "As " + themes_.current()->name + " brings them";
    } else if (*now == settings::SoundSet::Iisu) {
        note = "iiSU's, from " + iisuSounds()->file().filename().string();
    }
    return ui::makeChoice(interfaceSoundsId, "Interface sounds", note, std::move(choices));
}

ui::SettingsRow InterfaceSoundSettings::musicRow() const {
    const std::shared_ptr<const theme::InterfaceTheme> theme = themes_.current();
    const std::string note = theme->music.empty() ? theme->name + " brings no music"
                                                  : theme->name + "'s loop behind the menus";
    return ui::makeToggle(menuMusicId, "Menu music", note, preferences_.values().menuMusic);
}

bool InterfaceSoundSettings::owns(const std::string& id) {
    return id == interfaceSoundsId || id == menuMusicId;
}

void InterfaceSoundSettings::run(const std::string& id, int step) {
    settings::Settings& values = preferences_.values();
    if (id == menuMusicId) {
        values.menuMusic = !values.menuMusic;
    } else {
        values.interfaceSounds = ui::stepped(offered(), playing(), step);
    }
    preferences_.save();
    changed_();
}

} // namespace opensu::app
