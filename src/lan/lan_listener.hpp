// lan_listener — this openSU's listener on the LAN (`lucent::http::Server` on every IPv4
// interface, never the loopback control port). Open to anyone: `GET /lan/hello` (the id and name,
// all an unpaired openSU learns) and the three pairing calls. Everything else is for paired peers
// only, signed (`request_signature`) and answered signed: `GET /lan/library` (the shared
// listing), `GET /lan/cover/<token>` (one shared cover) and `POST /lan/unpair` (the peer forgets
// this one, and this one it). A request that fails its signature, comes late or repeats is
// refused with 403.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "host_pairing.hpp"
#include "lucent/http.h"
#include "peer_book.hpp"
#include "request_signature.hpp"
#include "shared_library.hpp"

namespace opensu::lan {

class LanListener {
  public:
    struct Sources {
        PeerBook& book;
        HostPairing& pairing;
        const SharedLibrary& library;
        /// The name this openSU goes by now.
        std::function<std::string()> name;
        /// A peer unpaired itself; called on the listener's thread.
        std::function<void(const Peer&)> unpaired;
    };

    /// Listens on `port`, or a free one when it is 0, once started.
    LanListener(Sources sources, std::uint16_t port);
    ~LanListener();
    LanListener(const LanListener&) = delete;
    LanListener& operator=(const LanListener&) = delete;
    LanListener(LanListener&&) = delete;
    LanListener& operator=(LanListener&&) = delete;

    /// Binds; false when the port cannot be had.
    [[nodiscard]] bool start();
    /// Stops listening and waits for the requests under way.
    void stop();
    [[nodiscard]] std::uint16_t port() const noexcept;

    /// Routes a request; exposed so the routes are testable without a socket.
    [[nodiscard]] lucent::http::Response handle(const lucent::http::Request& request);

  private:
    /// A request that passed its signature: from whom, with what secret, under what nonce.
    struct Signed {
        Peer peer;
        crypto::Bytes secret;
        std::string nonce;
    };

    [[nodiscard]] lucent::http::Response pair(const lucent::http::Request& request);
    [[nodiscard]] lucent::http::Response reveal(const lucent::http::Request& request);
    [[nodiscard]] lucent::http::Response pairStatus(const lucent::http::Request& request);
    /// The signed routes, once `verified` holds.
    [[nodiscard]] lucent::http::Response peerRoute(const lucent::http::Request& request,
                                                   const Signed& verified);
    [[nodiscard]] std::optional<Signed> verify(const lucent::http::Request& request);

    Sources sources_;
    ReplayGuard replay_;
    /// Last, so its threads stop before what they read goes.
    lucent::http::Server server_;
};

} // namespace opensu::lan
