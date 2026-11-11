#include "zero_tier.hpp"

#include <optional>
#include <string>
#include <utility>

#include "fileio/read_whole.hpp"
#include "lucent/log.h"
#include "lucent/p2p/zerotier.h"

namespace opensu::online {

ZeroTier::ZeroTier(std::filesystem::path storage, std::filesystem::path roots)
    : storage_{std::move(storage)}, roots_{std::move(roots)} {
}

ZeroTier::~ZeroTier() = default;

bool ZeroTier::built() noexcept {
    return true;
}

void ZeroTier::start() {
    if (tried_) {
        return;
    }
    tried_ = true;
    lucent::p2p::ZeroTierConfig config{
        .storage = storage_, .roots = {}, .port = 0, .network_port = zeroTierPort};
    if (!roots_.empty()) {
        std::string why;
        std::optional<std::string> roots = fileio::readWhole(roots_, why);
        if (!roots) {
            lucent::log(lucent::Level::Warn, "online",
                        "no ZeroTier root set: " + why + "; playing direct only");
            return;
        }
        config.roots = std::move(*roots);
    }
    std::string error;
    node_ = lucent::p2p::ZeroTierNode::start(config, error);
    if (!node_) {
        lucent::log(lucent::Level::Warn, "online",
                    "no ZeroTier: " + error + "; playing direct only");
    }
}

} // namespace opensu::online
