// login_copies — the owner's half of a paired openSU copying this openSU's Steam sign-in: a
// `PeerAsk` put to the player (Decline or Allow). The sign-in is what fumarole exports (`Export`),
// read when the ask comes, so one with nothing to give is refused before the player is asked, and
// again when an allowed asker collects its verdict, which carries it. It is a secret: never logged
// or kept here. Safe from any thread: the LAN listener's and the loop.
#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "peer_ask.hpp"
#include "peer_book.hpp"
#include "wire.hpp"

namespace opensu::lan {

class LoginCopies {
  public:
    using Clock = std::chrono::steady_clock;
    /// This openSU's Steam sign-in, a secret; nothing with why there is none. Called on the
    /// listener's thread; empty when this openSU has no fumarole.
    using Export = std::function<std::optional<std::string>(std::string& why)>;

    explicit LoginCopies(Export exported) : exported_{std::move(exported)} {
    }

    /// `peer` asks for the Steam sign-in. False with `refusal` when there is none to give or
    /// another ask waits for the player; the same ask again keeps its place.
    [[nodiscard]] bool ask(const Peer& peer, Clock::time_point now, std::string& refusal);
    /// The verdict on `peerId`'s ask, with the sign-in once allowed; nothing when no such ask is
    /// under way. An allowed sign-in that cannot be read is declined with why.
    [[nodiscard]] std::optional<wire::LoginVerdict> status(std::string_view peerId,
                                                           Clock::time_point now);
    /// The openSU whose ask waits for the player.
    [[nodiscard]] std::optional<Peer> question(Clock::time_point now) const;
    /// The player's answer to `peerId`'s ask; false when it is no longer asked.
    [[nodiscard]] bool decide(std::string_view peerId, bool allow, Clock::time_point now);

  private:
    Export exported_;
    PeerAsk<std::monostate> asks_;
};

} // namespace opensu::lan
