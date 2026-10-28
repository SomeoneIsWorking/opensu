#include "artwork_delivery.hpp"

namespace opensu::app {

void ArtworkDelivery::restart() {
    prioritised_.clear();
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
