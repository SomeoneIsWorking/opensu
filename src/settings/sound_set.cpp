#include "sound_set.hpp"

#include <algorithm>

namespace opensu::settings {

std::string_view key(SoundSet choice) noexcept {
    switch (choice) {
    case SoundSet::Marimba:
        return "marimba";
    case SoundSet::Glass:
        return "glass";
    case SoundSet::Pluck:
        return "pluck";
    case SoundSet::Iisu:
        return "iisu";
    }
    return {};
}

std::optional<SoundSet> soundSetOf(std::string_view spelling) noexcept {
    // Before openSU shipped more than one set, its own was "opensu".
    if (spelling == "opensu") {
        return SoundSet::Marimba;
    }
    const auto found = std::ranges::find_if(allSoundSets, [spelling](SoundSet choice) {
        return key(choice) == spelling;
    });
    return found == allSoundSets.end() ? std::nullopt : std::optional{*found};
}

std::string_view label(SoundSet choice) noexcept {
    switch (choice) {
    case SoundSet::Marimba:
        return "Marimba";
    case SoundSet::Glass:
        return "Glass";
    case SoundSet::Pluck:
        return "Pluck";
    case SoundSet::Iisu:
        return "iiSU";
    }
    return {};
}

SoundSet shippedSet(SoundSet choice) noexcept {
    return choice == SoundSet::Iisu ? SoundSet::Marimba : choice;
}

} // namespace opensu::settings
