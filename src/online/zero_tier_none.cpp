#include "zero_tier.hpp"

#include <utility>

namespace opensu::online {

ZeroTier::ZeroTier(std::filesystem::path storage, std::filesystem::path roots)
    : storage_{std::move(storage)}, roots_{std::move(roots)} {
}

ZeroTier::~ZeroTier() = default;

bool ZeroTier::built() noexcept {
    return false;
}

void ZeroTier::start() {
    tried_ = true;
}

} // namespace opensu::online
