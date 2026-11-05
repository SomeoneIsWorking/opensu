// lan_listener — this openSU's listener on the LAN (`lucent::http::Server` on every IPv4
// interface, never the loopback control port). Open to anyone: `GET /lan/hello` (the id and name,
// all an unpaired openSU learns) and the three pairing calls. Everything else is for paired peers
// only, signed, its body and answer sealed (`request_signature`): `GET /lan/library` (the shared
// listing), `GET /lan/cover/<token>` (one shared cover), `POST /lan/unpair` (the peer forgets this
// one, and this one it), the profiles (`GET /lan/profiles`, a copy asked of the player through
// `ProfileCopies`: `POST /lan/profiles/copy`, `/copy/status`, and `/unlink`), the save routes of a
// profile linked with the peer (`save_sync::SaveSync`: `/lan/saves/status`, `/get`, `/news`,
// `/synced`), `POST /lan/play`, which asks to play a listed game here over remote play (`play`),
// and ROM import's routes (`RomShelf`: `GET /lan/roms`, `POST /lan/roms/file`, `/chunk`). A request
// that fails its signature, comes late, repeats or does not open is refused with 403, as are the
// save routes of a profile not linked with the peer.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "host_pairing.hpp"
#include "lucent/http.h"
#include "pairing_keys.hpp"
#include "peer_book.hpp"
#include "profile_copies.hpp"
#include "profiles/profile_book.hpp"
#include "request_signature.hpp"
#include "rom_shelf.hpp"
#include "save_sync/save_sync.hpp"
#include "shared_library.hpp"

namespace opensu::lan {

class LanListener {
  public:
    struct Sources {
        PeerBook& book;
        HostPairing& pairing;
        const SharedLibrary& library;
        save_sync::SaveSync& saves;
        /// The profiles here: the save routes answer only for one linked with the caller.
        profiles::ProfileBook& profiles;
        /// The copies of them asked for, each put to the player.
        ProfileCopies& copies;
        RomShelf& roms;
        /// The name this openSU goes by now.
        std::function<std::string()> name;
        /// A peer unpaired itself; called on the listener's thread.
        std::function<void(const Peer&)> unpaired;
        /// A peer asks to play `game`, which this one lists, here as `profile`: the one the peer
        /// asked for when it is linked with the peer, else the first; `keys` seal that session.
        /// The UDP port its stream takes packets on, or nothing with `refusal`. Called on the
        /// listener's thread.
        std::function<std::optional<std::uint16_t>(const Peer& peer, const std::string& game,
                                                   const std::string& profile,
                                                   const StreamKeys& keys, std::string& refusal)>
            play;
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
    /// A request that passed its signature: from whom, with what secret, and what its seals are
    /// keyed by.
    struct Signed {
        Peer peer;
        secrets::crypto::Bytes secret;
        std::string nonce;
        std::int64_t time{0};
    };

    [[nodiscard]] lucent::http::Response pair(const lucent::http::Request& request);
    [[nodiscard]] lucent::http::Response reveal(const lucent::http::Request& request);
    [[nodiscard]] lucent::http::Response pairStatus(const lucent::http::Request& request);
    /// The signed routes, once `verified` holds.
    [[nodiscard]] lucent::http::Response peerRoute(const lucent::http::Request& request,
                                                   const Signed& verified);
    /// The save routes, `body` opened.
    [[nodiscard]] lucent::http::Response savesRoute(std::string_view path, std::string_view body,
                                                    const Signed& verified);
    /// `/lan/roms/file` and `/chunk`, `body` opened.
    [[nodiscard]] lucent::http::Response romsRoute(std::string_view path, std::string_view body,
                                                   const Signed& verified);
    /// `/lan/profiles/copy`, `/copy/status` and `/unlink`, `body` opened.
    [[nodiscard]] lucent::http::Response profilesRoute(std::string_view path, std::string_view body,
                                                       const Signed& verified);
    /// `/lan/play`, `body` opened.
    [[nodiscard]] lucent::http::Response playRoute(std::string_view body, const Signed& verified);
    [[nodiscard]] std::optional<Signed> verify(const lucent::http::Request& request);

    Sources sources_;
    ReplayGuard replay_;
    /// Last, so its threads stop before what they read goes.
    lucent::http::Server server_;
};

} // namespace opensu::lan
