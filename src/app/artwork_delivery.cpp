#include "artwork_delivery.hpp"

#include <filesystem>

#include "artwork/iisu_assets.hpp"

namespace opensu::app {

void ArtworkDelivery::restart() {
    prioritised_.clear();
}

void ArtworkDelivery::loadStoredSounds() {
    for (const audio::Effect effect : audio::allEffects) {
        if (const std::filesystem::path file = store_.storedAsset(artwork::soundAsset(effect));
            !file.empty()) {
            sounds_.load(effect, file);
        }
    }
}

void ArtworkDelivery::service() {
    for (const artwork::Fetched& fetched : fetcher_.take()) {
        if (!fetched.saved) {
            shell_.grid().artworkSettled(fetched.id);
            if (fetched.kind == artwork::Fetched::Kind::Game) {
                hooks_.coverMissing(fetched.id);
            }
            continue;
        }
        if (fetched.kind == artwork::Fetched::Kind::Sound) {
            if (const std::optional<audio::Effect> effect = audio::effectOfFile(fetched.id)) {
                sounds_.load(*effect, fetched.artwork);
            }
            continue;
        }
        if (fetched.kind == artwork::Fetched::Kind::Names) {
            hooks_.namesArrived();
            continue;
        }
        hooks_.coverArrived(fetched.id, fetched.artwork);
    }
    // The queue goes to the games on screen first.
    std::vector<std::string> onScreen = shell_.grid().downloadingOnScreen();
    if (onScreen != prioritised_) {
        fetcher_.prioritize(onScreen);
        prioritised_ = std::move(onScreen);
    }
}

} // namespace opensu::app
