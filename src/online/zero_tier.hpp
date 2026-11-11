// zero_tier — Play online's embedded ZeroTier node: started when the player first opens Play
// online, so it has reached its network by the time a code is made or pasted, and kept for the
// run. A build without ZeroTier (OPENSU_BUILD_ZEROTIER=OFF) has none, and the meeting dials direct.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include "lucent/p2p/zerotier_paths.h"

namespace opensu::online {

/// openSU's ad-hoc network, and the UDP port on it: every openSU's, so a code needs only the node.
inline constexpr std::uint16_t zeroTierPort = 41027;

class ZeroTier {
  public:
    /// The node keeps its identity in `storage`; `roots` names a root set file, empty for
    /// ZeroTier's own.
    ZeroTier(std::filesystem::path storage, std::filesystem::path roots);
    ~ZeroTier();
    ZeroTier(const ZeroTier&) = delete;
    ZeroTier& operator=(const ZeroTier&) = delete;
    ZeroTier(ZeroTier&&) = delete;
    ZeroTier& operator=(ZeroTier&&) = delete;

    /// Whether this build carries ZeroTier.
    [[nodiscard]] static bool built() noexcept;
    /// Starts the node, once; a failure is logged and leaves Play online direct.
    void start();
    /// The node for a meeting; none before `start`, after a failed one, or without ZeroTier.
    [[nodiscard]] const lucent::p2p::ZeroTierPaths* paths() const noexcept {
        return node_.get();
    }

  private:
    std::filesystem::path storage_;
    std::filesystem::path roots_;
    bool tried_{false};
    std::unique_ptr<lucent::p2p::ZeroTierPaths> node_;
};

} // namespace opensu::online
