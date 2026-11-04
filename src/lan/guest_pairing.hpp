// guest_pairing — this openSU pairing with a host it found, on a thread of its own: it commits to a
// fresh X25519 key, takes the host's, reveals its own, shows the code both screens show and polls
// the host until the player there accepts or declines (`patience`). Accepted, with the verdict
// signed by the pairing's secret, the host and the secret go into the book.
#pragma once

#include <cstdint>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

#include "dns_sd.hpp"
#include "net/web_client.hpp"
#include "peer_book.hpp"

namespace opensu::lan {

class GuestPairing {
  public:
    enum class Phase : std::uint8_t {
        /// Exchanging keys with the host.
        Asking,
        /// The code is shown; the player at the host has not answered yet.
        Waiting,
        Paired,
        Declined,
        Failed,
        Cancelled,
    };

    struct State {
        Phase phase{Phase::Asking};
        std::string hostName;
        /// The code both screens show, once there is one.
        std::string code;
        /// Why it failed.
        std::string problem;
    };

    /// Pairs as `self` (this openSU's id and name) with `host`, keeping it in `book`.
    GuestPairing(const net::WebClient& web, PeerBook& book, Peer self, Sighting host);
    ~GuestPairing();
    GuestPairing(const GuestPairing&) = delete;
    GuestPairing& operator=(const GuestPairing&) = delete;
    GuestPairing(GuestPairing&&) = delete;
    GuestPairing& operator=(GuestPairing&&) = delete;

    [[nodiscard]] State state() const;
    [[nodiscard]] const Sighting& host() const noexcept {
        return host_;
    }
    /// Stops asking; the host's question lapses on its own.
    void cancel();
    /// Whether it ended, one way or another.
    [[nodiscard]] bool done() const;

  private:
    void run(const std::stop_token& stop);
    /// The exchange itself; the phase it ended in, `problem` set when it failed.
    Phase pair(const std::stop_token& stop, std::string& problem);
    void enter(Phase phase, std::string problem = {});

    const net::WebClient& web_;
    PeerBook& book_;
    Peer self_;
    Sighting host_;
    mutable std::mutex mutex_;
    State state_;
    /// Last, so it stops before what it uses goes.
    std::jthread worker_;
};

} // namespace opensu::lan
