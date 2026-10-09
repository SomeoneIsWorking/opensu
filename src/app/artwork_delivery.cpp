#include "artwork_delivery.hpp"

#include <filesystem>

#include "artwork/iisu_assets.hpp"

namespace opensu::app {

void ArtworkDelivery::restart() {
    prioritised_.clear();
    loadStoredNavIcons();
}

void ArtworkDelivery::loadStoredSounds() {
    for (const audio::Effect effect : audio::allEffects) {
        if (const std::filesystem::path file = store_.storedAsset(artwork::soundAsset(effect));
            !file.empty()) {
            sounds_.load(effect, file);
        }
    }
}

void ArtworkDelivery::loadStoredNavIcons() {
    for (const artwork::NavIcon icon : artwork::allNavIcons) {
        if (const std::filesystem::path file = store_.storedAsset(artwork::navAsset(icon));
            !file.empty()) {
            shell_.setNavIcon(icon.section, icon.selected, file);
        }
    }
}

void ArtworkDelivery::service() {
    for (const artwork::Fetched& fetched : fetcher_.take()) {
        if (!fetched.saved) {
            shell_.artworkSettled(fetched.id);
            continue;
        }
        if (fetched.kind == artwork::Fetched::Kind::Sound) {
            if (const std::optional<audio::Effect> effect = audio::effectOfFile(fetched.id)) {
                sounds_.load(*effect, fetched.artwork);
            }
            continue;
        }
        if (fetched.kind == artwork::Fetched::Kind::NavIcon) {
            if (const std::optional<artwork::NavIcon> icon = artwork::navIconOfFile(fetched.id)) {
                shell_.setNavIcon(icon->section, icon->selected, fetched.artwork);
            }
            continue;
        }
        if (fetched.kind == artwork::Fetched::Kind::Names) {
            namesArrived_();
            continue;
        }
        if (fetched.kind == artwork::Fetched::Kind::Glyph) {
            shell_.setGlyph(fetched.id, fetched.artwork);
            continue;
        }
        if (fetched.kind == artwork::Fetched::Kind::Console) {
            shell_.setConsoleArtwork(fetched.id, fetched.artwork);
            continue;
        }
        shell_.setArtwork(fetched.id, fetched.artwork);
    }
    // The queue goes to the games on screen first.
    std::vector<std::string> onScreen = shell_.downloadingOnScreen();
    if (onScreen != prioritised_) {
        fetcher_.prioritize(onScreen);
        prioritised_ = std::move(onScreen);
    }
}

} // namespace opensu::app
