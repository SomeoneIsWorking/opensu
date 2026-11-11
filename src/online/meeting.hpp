// meeting — two people getting from a code to a game, on a thread of its own: Lucent's
// `p2p::Connector` takes the code to an authenticated path, then over its `ReliableLink` the ask,
// the friend's answer, both saves and the word to go. The owner looks at `view`, passes on what the
// player does and takes the `Agreement` when it is `Ready`, by then with the socket closed so a
// runner can bind its port, or a ZeroTier path in a bridge. Every wait has a limit and ends in
// words that say what to do next.
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include "lan/wire.hpp"
#include "lucent/p2p/connector.h"
#include "lucent/p2p/path_bridge.h"
#include "lucent/p2p/reliable_link.h"
#include "lucent/p2p/zerotier_paths.h"

namespace opensu::online {

/// openSU's names for its codes, and the label that keeps its keys its own.
inline constexpr lucent::p2p::CodePrefixes codePrefixes{"OSU", "OSR"};
inline constexpr std::string_view keyLabel = "opensu-online";

/// The problem with a pasted code, in words for the player.
[[nodiscard]] std::string_view textOf(lucent::p2p::CodeProblem problem) noexcept;

/// Where a meeting looks for what it needs; a test or a lab points these elsewhere.
struct Network {
    lucent::p2p::DirectNetwork direct;
    /// The shell's ZeroTier node, which outlives the meeting; none dials direct only.
    const lucent::p2p::ZeroTierPaths* zerotier{nullptr};
};

/// How long each wait lasts: Lucent's to the connection, then the agreement's.
struct Patience : lucent::p2p::Patience {
    /// A question to a player, and the whole of what follows the connection.
    std::chrono::seconds agreeing{std::chrono::minutes{3}};
};

/// The kinds of message the shells send over the link, each capped.
enum class LinkKind : std::uint8_t {
    /// The ROM, its console and what must match (`lan::wire::NetplayAsk`).
    Ask = 1,
    /// Yes, or no and why.
    Answer = 2,
    /// One player's save.
    Save = 3,
    /// The session's clock and delay; both start.
    Go = 4,
};

/// What both start a session with.
struct Agreement {
    /// The local port the runner binds, and where the other machine is.
    std::uint16_t port{0};
    lucent::net::Endpoint peer;
    /// The keys the runners' session seals under: this side's, then the other's.
    lucent::p2p::crypto::Bytes sendKey;
    lucent::p2p::crypto::Bytes receiveKey;
    std::string hostSave;
    std::string friendSave;
    lan::wire::NetplayClock clock;
    /// Frames of input delay, measured from the round trip.
    unsigned delay{2};
    /// Frames a runner may run past the input it has; 0 for lockstep.
    unsigned ahead{0};
    /// The router's mapping, while the game runs.
    std::unique_ptr<lucent::p2p::PortLease> lease;
    /// A ZeroTier game's path, which only the shell's node can carry, while the game runs; `peer`
    /// is then its end on loopback.
    std::unique_ptr<lucent::p2p::PathBridge> bridge;
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
    Meeting(Network network, Patience patience, lan::wire::NetplayAsk ask, std::string save,
            bool rollback);
    /// A friend who pasted `invite`.
    Meeting(Network network, Patience patience, lucent::p2p::Invite invite);
    ~Meeting();
    Meeting(const Meeting&) = delete;
    Meeting& operator=(const Meeting&) = delete;
    Meeting(Meeting&&) = delete;
    Meeting& operator=(Meeting&&) = delete;

    [[nodiscard]] View view() const;
    /// The host's player pasted `code`; nothing when it is taken, else what is wrong with it.
    [[nodiscard]] std::optional<lucent::p2p::CodeProblem> reply(std::string_view code);
    /// The friend's player said yes, offering `save`.
    void accept(std::string save);
    /// The friend said no, or cannot play; the host is told `why`.
    void decline(std::string why);
    /// The session, once, at `Ready`.
    [[nodiscard]] std::unique_ptr<Agreement> takeAgreement();

    /// `failure` in words for the player.
    [[nodiscard]] static std::string textOf(Failure failure);
    /// What a host with a core that may roll back asks for, from the frames `delayFor` gives: the
    /// least delay, and at least the rest as frames to run ahead.
    [[nodiscard]] static unsigned aheadFor(unsigned delay) noexcept;
    /// The input delay for a measured round trip.
    [[nodiscard]] static unsigned delayFor(std::chrono::milliseconds roundTrip) noexcept;

  private:
    /// What the player did, which the thread picks up.
    struct Inbox {
        std::optional<std::string> accepted;
        std::optional<std::string> declined;
    };
    using ReliableLink = lucent::p2p::ReliableLink;
    using LinkMessage = lucent::p2p::LinkMessage;

    [[nodiscard]] static lucent::p2p::ConnectorConfig configOf(Network network,
                                                               const Patience& patience);
    void run(const std::stop_token& stop);
    void meet(const std::stop_token& stop);
    /// The connector's view as this meeting's, until it is connected.
    [[nodiscard]] std::unique_ptr<lucent::p2p::Connection> connect(const std::stop_token& stop);
    void agreeAsHost(const std::stop_token& stop, ReliableLink& link,
                     const lan::wire::NetplayAsk& ask, Agreement& agreement);
    void agreeAsFriend(const std::stop_token& stop, ReliableLink& link, Agreement& agreement);
    [[nodiscard]] std::optional<LinkMessage> await(const std::stop_token& stop, ReliableLink& link,
                                                   LinkKind kind, Clock::time_point until);
    void drain(const std::stop_token& stop, ReliableLink& link, std::chrono::milliseconds most);
    void fail(Failure failure, std::string refusal = {});
    void show(Step step);
    [[nodiscard]] Inbox takeInbox();
    [[nodiscard]] bool failed() const;

    Patience patience_;
    /// The host's.
    std::optional<lan::wire::NetplayAsk> ask_;
    std::string save_;
    bool rollback_{false};
    std::unique_ptr<lucent::p2p::Connector> connector_;

    mutable std::mutex mutex_;
    View view_;
    Inbox inbox_;
    std::unique_ptr<Agreement> agreement_;
    std::jthread worker_;
};

} // namespace opensu::online
