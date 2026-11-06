// meeting — two people getting from a code to a game, on a thread of its own: this machine's
// addresses found (STUN, the router), the code shown, the other machine found (`Rendezvous`), then
// over `ReliableLink` the ask, the friend's answer, both saves and the word to go. The owner looks
// at `view`, passes on what the player does and takes the `Agreement` when it is `Ready`, by then
// with the socket closed so a runner can bind its port. Every wait has a limit and ends in words
// that say what to do next.
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "host_network.hpp"
#include "invite_code.hpp"
#include "lan/wire.hpp"
#include "port_lease.hpp"
#include "reliable_link.hpp"
#include "rendezvous.hpp"

namespace opensu::online {

/// Public STUN servers of two operators, so a NAT that varies its mapping shows.
inline constexpr std::string_view defaultStunServers =
    "stun.l.google.com:19302,stun.cloudflare.com:3478";

/// Where a meeting looks for what it needs; a test or a lab points these elsewhere.
struct Network {
    /// `host:port`, comma-separated.
    std::string stunServers{defaultStunServers};
    /// Instead of the system's.
    std::optional<std::vector<Interface>> interfaces;
    /// Whether to ask the router for a port at all.
    bool router{true};
    /// NAT-PMP's address; the default gateway when not set.
    std::optional<stream::UdpAddress> natPmp;
    /// Where SSDP searches go; the multicast group when not set.
    std::optional<stream::UdpAddress> ssdp;
};

/// How long each wait lasts.
struct Patience {
    /// A host's code stays good this long.
    std::chrono::seconds invite{std::chrono::minutes{10}};
    /// A friend tries the host alone this long before showing a reply code.
    std::chrono::milliseconds direct{std::chrono::seconds{3}};
    /// A friend keeps trying this long after pasting the invite.
    std::chrono::seconds friendTries{std::chrono::minutes{2}};
    /// Both punch this long once the host has the friend's reply.
    std::chrono::seconds punch{std::chrono::seconds{15}};
    /// A question to a player, and the whole of what follows the connection.
    std::chrono::seconds agreeing{std::chrono::minutes{3}};
};

/// What both start a session with.
struct Agreement {
    /// The local port the runner binds, and where the other machine is.
    std::uint16_t port{0};
    stream::UdpAddress peer;
    SessionKeys keys;
    std::string hostSave;
    std::string friendSave;
    lan::wire::NetplayClock clock;
    /// Frames of input delay, measured from the round trip.
    unsigned delay{2};
    /// The router's mapping, while the game runs.
    std::unique_ptr<PortLease> lease;
};

class Meeting {
  public:
    using Clock = std::chrono::steady_clock;

    enum class Step : std::uint8_t {
        /// Finding this machine's addresses.
        Gathering,
        /// The host's code is ready; nobody has arrived.
        Inviting,
        /// The friend is trying the host's addresses.
        Trying,
        /// The friend's reply code is ready; both keep trying.
        Replying,
        /// The host has the reply; both punch.
        Punching,
        /// Connected; the two sides are agreeing on the game.
        Agreeing,
        /// The friend's player must say yes or no to `asked`.
        Asking,
        /// `takeAgreement` holds the session.
        Ready,
        Failed,
    };
    enum class Failure : std::uint8_t {
        None,
        /// Punching ended without a way through.
        Unreachable,
        /// The code ran out.
        Lapsed,
        /// The other machine went away.
        Dropped,
        /// The other side, or this one, said no; `refusal` says why.
        Refused,
        /// No socket, or no network at all.
        NoNetwork,
    };

    struct View {
        Step step{Step::Gathering};
        Failure failure{Failure::None};
        /// The code to send: the invite, or the reply.
        std::string code;
        std::string refusal;
        /// This side's NAT will not let a strict one reach it.
        bool closed{false};
        /// What the host asks, at `Asking`.
        std::optional<lan::wire::NetplayAsk> asked;
    };

    /// A host offering `ask`, with this player's `save`.
    Meeting(Network network, Patience patience, lan::wire::NetplayAsk ask, std::string save);
    /// A friend who pasted `invite`.
    Meeting(Network network, Patience patience, Invite invite);
    ~Meeting();
    Meeting(const Meeting&) = delete;
    Meeting& operator=(const Meeting&) = delete;
    Meeting(Meeting&&) = delete;
    Meeting& operator=(Meeting&&) = delete;

    [[nodiscard]] View view() const;
    /// The host's player pasted `code`; nothing when it is taken, else what is wrong with it.
    [[nodiscard]] std::optional<CodeProblem> reply(std::string_view code);
    /// The friend's player said yes, offering `save`.
    void accept(std::string save);
    /// The friend said no, or cannot play; the host is told `why`.
    void decline(std::string why);
    /// The session, once, at `Ready`.
    [[nodiscard]] std::unique_ptr<Agreement> takeAgreement();

    /// `failure` in words for the player.
    [[nodiscard]] static std::string textOf(Failure failure);
    /// The input delay for a measured round trip.
    [[nodiscard]] static unsigned delayFor(std::chrono::milliseconds roundTrip) noexcept;

  private:
    /// What the player did, which the thread picks up.
    struct Inbox {
        std::optional<Reach> reply;
        std::optional<std::string> accepted;
        std::optional<std::string> declined;
    };
    struct Gathered;

    void run(const std::stop_token& stop);
    void meet(const std::stop_token& stop);
    [[nodiscard]] Gathered gather(const std::stop_token& stop,
                                  const stream::UdpSocket& socket) const;
    [[nodiscard]] std::optional<Found> find(const std::stop_token& stop,
                                            std::unique_ptr<stream::UdpSocket>& socket,
                                            Gathered& gathered);
    void agreeAsHost(const std::stop_token& stop, ReliableLink& link,
                     const lan::wire::NetplayAsk& ask, Agreement& agreement);
    void agreeAsFriend(const std::stop_token& stop, ReliableLink& link, Agreement& agreement);
    [[nodiscard]] std::optional<LinkMessage> await(const std::stop_token& stop, ReliableLink& link,
                                                   LinkKind kind, Clock::time_point until);
    void drain(const std::stop_token& stop, ReliableLink& link, std::chrono::milliseconds most);
    void fail(Failure failure, std::string refusal = {});
    void show(Step step);
    void offer(std::string code, bool closed, Step step);
    [[nodiscard]] Inbox takeInbox();
    [[nodiscard]] bool failed() const;

    Network network_;
    Patience patience_;
    /// The host's.
    std::optional<lan::wire::NetplayAsk> ask_;
    std::string save_;
    /// The friend's; for the host, its secret alone until a reach goes in.
    Invite invite_;

    mutable std::mutex mutex_;
    View view_;
    Inbox inbox_;
    std::unique_ptr<Agreement> agreement_;
    std::jthread worker_;
};

} // namespace opensu::online
