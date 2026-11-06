// rendezvous — two machines finding each other from a code, on one UDP socket. What they send
// before they share session keys is sealed with a key from the code's secret under a random nonce:
// the friend's `Hello` and the host's `Welcome` carry ephemeral X25519 keys, and the host's `Knock`
// opens its NAT toward the addresses in a reply code. Each side's datagrams to the other punch its
// own NAT, but a friend waiting on a reply code keeps its datagrams short of the host's: one that
// arrives first leaves a Linux NAT with an entry that moves the host's own port when it answers.
// The host takes the friend as its peer only when a datagram opens under the session keys, and then
// the code is spent. Whatever does not open is dropped.
#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "invite_code.hpp"
#include "secrets/crypto.hpp"
#include "stream/packet_seal.hpp"
#include "stream/udp_socket.hpp"

namespace opensu::online {

/// One key per sender and purpose, so no counter repeats under a key: `link` for the shells' talk
/// before the game, `game` for the session the runners play.
struct SessionKeys {
    secrets::crypto::Bytes linkHost;
    secrets::crypto::Bytes linkFriend;
    secrets::crypto::Bytes gameHost;
    secrets::crypto::Bytes gameFriend;

    bool operator==(const SessionKeys&) const = default;
};

/// Who found the other.
struct Found {
    stream::UdpAddress peer;
    SessionKeys keys;
};

class Rendezvous {
  public:
    using Clock = std::chrono::steady_clock;

    enum class Role : std::uint8_t { Host, Friend };

    /// Sends on `socket`, which must outlive this. `secret` is the invite's.
    Rendezvous(const stream::UdpSocket& socket, Role role, std::span<const std::uint8_t> secret);
    ~Rendezvous();
    Rendezvous(const Rendezvous&) = delete;
    Rendezvous& operator=(const Rendezvous&) = delete;
    Rendezvous(Rendezvous&&) = delete;
    Rendezvous& operator=(Rendezvous&&) = delete;

    /// Adds where the other machine may be, to send to from now on.
    void aim(const std::vector<stream::UdpAddress>& addresses);
    /// Sends only far enough to hold this side's NAT open, until the other machine is heard.
    void keepNear();
    /// Sends what is due and takes what came, waiting at most `wait`.
    void step(std::chrono::milliseconds wait);
    /// The other machine, once found; `step` does nothing more after that.
    [[nodiscard]] const std::optional<Found>& found() const noexcept {
        return found_;
    }
    /// Datagrams dropped for not opening, or for arriving once the code was spent.
    [[nodiscard]] std::uint64_t dropped() const noexcept {
        return dropped_;
    }
    /// The last STUN datagram that came, once.
    [[nodiscard]] std::string takeStun();

  private:
    /// A friend who said Hello, not yet proved by a datagram under the session keys.
    struct Caller {
        SessionKeys keys;
        std::unique_ptr<stream::PacketOpener> opener;
    };

    [[nodiscard]] std::string sealed(std::uint8_t kind, std::string_view plain) const;
    [[nodiscard]] std::optional<std::string> opened(std::uint8_t kind,
                                                    std::string_view datagram) const;
    void take(std::string_view datagram, const stream::UdpAddress& from);
    void takeHello(std::string_view datagram, const stream::UdpAddress& from);
    void takeWelcome(std::string_view datagram, const stream::UdpAddress& from);
    void takeKnock(std::string_view datagram, const stream::UdpAddress& from);
    void takeSealed(std::string_view datagram, const stream::UdpAddress& from);
    void sendDue();
    [[nodiscard]] SessionKeys keysWith(std::span<const std::uint8_t> shared,
                                       std::span<const std::uint8_t> hostKey,
                                       std::span<const std::uint8_t> friendKey) const;

    const stream::UdpSocket& socket_;
    Role role_;
    secrets::crypto::Bytes secret_;
    secrets::crypto::Bytes helloKey_;
    secrets::crypto::KeyPair own_;
    std::vector<stream::UdpAddress> targets_;
    std::map<secrets::crypto::Bytes, Caller> callers_;
    std::optional<Found> found_;
    Clock::time_point nextSend_{};
    bool near_{false};
    std::uint64_t dropped_{0};
    std::vector<char> buffer_;
    std::string stun_;
};

} // namespace opensu::online
