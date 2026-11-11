#include "meeting.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "fileio/little_endian.hpp"
#include "lan/peer_book.hpp"
#include "lucent/log.h"
#include "nat_pmp.hpp"
#include "secrets/secret.hpp"
#include "stun.hpp"
#include "upnp_igd.hpp"

namespace opensu::online {
namespace {

using namespace std::chrono_literals;

constexpr auto stunResend = 300ms;
constexpr auto stunWait = 2s;
/// Keeps the NAT's mapping alive while a code is out.
constexpr auto stunKeepEvery = 15s;
constexpr auto ssdpWait = 1500ms;
constexpr const char* ssdpGroup = "239.255.255.250";
constexpr auto pumpWait = 20ms;
constexpr auto meetWait = 50ms;
constexpr auto goAckWait = 2s;
constexpr auto goLinger = 500ms;
constexpr auto frameMs = 1000.0 / 60.0;
constexpr unsigned leastDelay = 2;
constexpr unsigned mostDelay = 12;
constexpr std::size_t goBytes = 1 + 1 + 8 + 4;
/// A rollback session's reach, and the most a runner takes.
constexpr unsigned leastAhead = 4;
constexpr unsigned mostAhead = 12;
constexpr std::size_t mostRefusal = 200;
constexpr std::string_view yes = "Y";
constexpr std::string_view no = "N";

/// `host:port`, or nothing.
std::optional<stream::UdpAddress> serverOf(std::string_view text) {
    const std::size_t colon = text.rfind(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }
    std::uint16_t port = 0;
    const char* end = text.data() + text.size();
    const auto [parsed, failure] = std::from_chars(text.data() + colon + 1, end, port);
    if (failure != std::errc{} || parsed != end) {
        return std::nullopt;
    }
    return resolve(std::string{text.substr(0, colon)}, port);
}

std::vector<stream::UdpAddress> serversOf(std::string_view list) {
    std::vector<stream::UdpAddress> servers;
    for (const auto part : std::views::split(list, ',')) {
        if (const auto server = serverOf(std::string_view{part.begin(), part.end()})) {
            servers.push_back(*server);
        }
    }
    return servers;
}

/// The address each server saw, from one socket. Empty for one that did not answer.
std::vector<std::optional<stream::UdpAddress>>
askServers(const std::stop_token& stop, const stream::UdpSocket& socket,
           const std::vector<stream::UdpAddress>& servers) {
    std::vector<StunId> ids(servers.size());
    std::ranges::generate(ids, newStunId);
    std::vector<std::optional<stream::UdpAddress>> seen(servers.size());
    std::vector<char> buffer(stream::largestDatagram);
    const auto until = Meeting::Clock::now() + stunWait;
    auto nextSend = Meeting::Clock::time_point{};
    while (Meeting::Clock::now() < until && !stop.stop_requested() &&
           std::ranges::any_of(seen, [](const auto& answer) {
               return !answer.has_value();
           })) {
        if (Meeting::Clock::now() >= nextSend) {
            nextSend = Meeting::Clock::now() + stunResend;
            for (std::size_t at = 0; at < servers.size(); ++at) {
                if (!seen[at]) {
                    socket.send(servers[at], stunRequest(ids[at]));
                }
            }
        }
        stream::UdpAddress from;
        const std::optional<std::size_t> length = socket.receive(buffer, from, meetWait);
        if (!length) {
            continue;
        }
        for (std::size_t at = 0; at < servers.size(); ++at) {
            if (from == servers[at]) {
                if (const auto mapped = stunMapped({buffer.data(), *length}, ids[at])) {
                    seen[at] = mapped;
                }
            }
        }
    }
    return seen;
}

std::unique_ptr<PortLease> mapPort(const std::stop_token& stop, const Network& network,
                                   std::uint16_t port, const std::vector<std::uint32_t>& locals,
                                   std::optional<std::uint32_t> gateway) {
    std::optional<stream::UdpAddress> natPmp = network.natPmp;
    if (!natPmp && gateway) {
        natPmp = stream::UdpAddress{.ip = *gateway, .port = natPmpPort};
    }
    if (natPmp) {
        auto mapper = std::make_unique<NatPmp>(*natPmp);
        if (const std::optional<Mapping> mapping = mapper->open(port, askedLease)) {
            lucent::info("online", "NAT-PMP forwards a port");
            return std::make_unique<PortLease>(std::move(mapper), port, *mapping);
        }
    }
    if (locals.empty()) {
        return nullptr;
    }
    const std::optional<stream::UdpAddress> ssdp =
        network.ssdp ? network.ssdp : stream::udpAddress(ssdpGroup, ssdpPort);
    std::unique_ptr<UpnpIgd> igd =
        ssdp ? UpnpIgd::discover(stop, *ssdp, locals.front(), ssdpWait) : nullptr;
    if (igd) {
        if (const std::optional<Mapping> mapping = igd->open(port, askedLease)) {
            lucent::info("online", "UPnP forwards a port");
            return std::make_unique<PortLease>(std::move(igd), port, *mapping);
        }
    }
    return nullptr;
}

std::string goBody(unsigned delay, unsigned ahead, const lan::wire::NetplayClock& clock) {
    std::string body(1, static_cast<char>(delay));
    body.push_back(static_cast<char>(ahead));
    fileio::appendLittleEndian(body, static_cast<std::uint64_t>(clock.epochMs), 8);
    fileio::appendLittleEndian(body, static_cast<std::uint32_t>(clock.utcOffset), 4);
    return body;
}

} // namespace

struct Meeting::Gathered {
    Reach reach;
    std::unique_ptr<PortLease> lease;
    /// The STUN server that answered, to keep the mapping alive with.
    std::optional<stream::UdpAddress> server;
};

Meeting::Meeting(Network network, Patience patience, lan::wire::NetplayAsk ask, std::string save,
                 bool rollback)
    : network_{std::move(network)}, patience_{patience}, ask_{std::move(ask)},
      save_{std::move(save)}, rollback_{rollback},
      invite_{.secret = secrets::crypto::randomBytes(secretBytes), .reach = {}},
      worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

Meeting::Meeting(Network network, Patience patience, Invite invite)
    : network_{std::move(network)}, patience_{patience}, invite_{std::move(invite)},
      worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

Meeting::~Meeting() {
    worker_.request_stop();
    worker_.join();
    secrets::wipeBytes(invite_.secret.data(), invite_.secret.size());
}

Meeting::View Meeting::view() const {
    const std::lock_guard lock{mutex_};
    return view_;
}

std::optional<CodeProblem> Meeting::reply(std::string_view code) {
    CodeProblem problem{};
    std::optional<Reach> reach = readReply(code, invite_.secret, problem);
    if (!reach) {
        return problem;
    }
    const std::lock_guard lock{mutex_};
    inbox_.reply = std::move(reach);
    return std::nullopt;
}

void Meeting::accept(std::string save) {
    const std::lock_guard lock{mutex_};
    inbox_.accepted = std::move(save);
}

void Meeting::decline(std::string why) {
    const std::lock_guard lock{mutex_};
    inbox_.declined = std::move(why);
}

std::unique_ptr<Agreement> Meeting::takeAgreement() {
    const std::lock_guard lock{mutex_};
    return std::move(agreement_);
}

Meeting::Inbox Meeting::takeInbox() {
    const std::lock_guard lock{mutex_};
    return std::exchange(inbox_, {});
}

void Meeting::fail(Failure failure, std::string refusal) {
    const std::lock_guard lock{mutex_};
    view_.step = Step::Failed;
    view_.failure = failure;
    view_.refusal = std::move(refusal);
    view_.code.clear();
    lucent::info("online", "no game: {}", textOf(failure));
}

bool Meeting::failed() const {
    const std::lock_guard lock{mutex_};
    return view_.step == Step::Failed;
}

void Meeting::show(Step step) {
    const std::lock_guard lock{mutex_};
    view_.step = step;
}

void Meeting::offer(std::string code, bool closed, Step step) {
    const std::lock_guard lock{mutex_};
    view_.step = step;
    view_.code = std::move(code);
    view_.closed = closed;
}

std::string Meeting::textOf(Failure failure) {
    switch (failure) {
    case Failure::None:
        return {};
    case Failure::Unreachable:
        return "Your two networks could not reach each other directly, and openSU uses no relay "
               "server. Install Tailscale or ZeroTier on both machines and join one network, then "
               "make a new code: it finds the way over that network by itself.";
    case Failure::Lapsed:
        return "The code ran out before anyone joined. Make a new one.";
    case Failure::Dropped:
        return "The other machine went away.";
    case Failure::Refused:
        return "The game did not start.";
    case Failure::NoNetwork:
        return "This machine has no network to play over.";
    }
    return {};
}

unsigned Meeting::delayFor(std::chrono::milliseconds roundTrip) noexcept {
    const double oneWay = static_cast<double>(roundTrip.count()) / 2.0;
    const auto frames = static_cast<unsigned>(std::ceil(oneWay / frameMs)) + 1;
    return std::clamp(frames, leastDelay, mostDelay);
}

unsigned Meeting::aheadFor(unsigned delay) noexcept {
    return std::max(delay - leastDelay, leastAhead);
}

void Meeting::run(const std::stop_token& stop) {
    try {
        meet(stop);
    } catch (const std::runtime_error& failure) {
        lucent::error("online", "the meeting cannot go on: {}", failure.what());
        fail(Failure::NoNetwork);
    }
}

Meeting::Gathered Meeting::gather(const std::stop_token& stop,
                                  const stream::UdpSocket& socket) const {
    Gathered gathered;
    gathered.reach.port = socket.port();
    std::optional<std::vector<lucent::net::Ipv4Interface>> listed = network_.interfaces;
    if (!listed) {
        std::string why;
        listed = lucent::net::ipv4_interfaces(why);
        if (!listed) {
            lucent::warn("online", "this machine's interfaces are unknown: {}", why);
            listed.emplace();
        }
    }
    const std::vector<lucent::net::Ipv4Interface>& found = *listed;
    const std::optional<std::uint32_t> gateway = defaultGateway();
    std::vector<std::uint32_t> locals = reachableLocals(found, gateway);
    gathered.reach.locals = locals;
    if (locals.size() > mostAddresses) {
        gathered.reach.locals.resize(mostAddresses);
    }
    const std::vector<stream::UdpAddress> servers = serversOf(network_.stunServers);
    const auto seen = askServers(stop, socket, servers);
    std::vector<stream::UdpAddress> answered;
    for (std::size_t at = 0; at < seen.size(); ++at) {
        if (const std::optional<stream::UdpAddress>& mapped = seen[at]) {
            gathered.server = gathered.server.value_or(servers[at]);
            if (std::ranges::find(answered, *mapped) == answered.end()) {
                answered.push_back(*mapped);
            }
        }
    }
    const bool varying = answered.size() > 1;
    if (!answered.empty()) {
        gathered.reach.publics.push_back(answered.front());
    }
    if (network_.router) {
        // A mapping points at the interface the router is on.
        std::erase_if(locals, [&found, gateway](std::uint32_t ip) {
            return gateway && std::ranges::none_of(found, [ip, gateway](const auto& interface) {
                       return addressOf(interface) == ip && onLinkWith(interface, gateway);
                   });
        });
        gathered.lease = mapPort(stop, network_, socket.port(), locals, gateway);
        if (gathered.lease) {
            gathered.reach.publics.insert(gathered.reach.publics.begin(),
                                          gathered.lease->external());
        }
    }
    gathered.reach.varying = varying && !gathered.lease;
    std::string kept;
    for (const std::uint32_t ip : gathered.reach.locals) {
        kept += (kept.empty() ? "" : " ") + stream::dotted(ip);
    }
    lucent::info("online", "{} public addresses, {} local ({}), mapping {}",
                 gathered.reach.publics.size(), gathered.reach.locals.size(), kept,
                 varying ? "varies by destination" : "steady");
    return gathered;
}

std::optional<Found> Meeting::find(const std::stop_token& stop,
                                   std::unique_ptr<stream::UdpSocket>& socket, Gathered& gathered) {
    const bool host = ask_.has_value();
    auto rendezvous = std::make_unique<Rendezvous>(
        *socket, host ? Rendezvous::Role::Host : Rendezvous::Role::Friend, invite_.secret);
    const Clock::time_point started = Clock::now();
    Clock::time_point until = started + (host ? patience_.invite : patience_.friendTries);
    auto nextKeep = started + stunKeepEvery;
    bool replying = false;
    bool punching = false;
    if (host) {
        invite_.reach = gathered.reach;
        offer(writeInvite(invite_), gathered.reach.varying, Step::Inviting);
    } else {
        rendezvous->aim(invite_.reach.addresses());
        show(Step::Trying);
    }
    while (!stop.stop_requested()) {
        rendezvous->step(meetWait);
        if (rendezvous->found()) {
            return rendezvous->found();
        }
        const Clock::time_point now = Clock::now();
        if (gathered.server && now >= nextKeep) {
            nextKeep = now + stunKeepEvery;
            socket->send(*gathered.server, stunRequest(newStunId()));
        }
        if (host) {
            if (std::optional<Reach> reach = takeInbox().reply) {
                rendezvous->aim(reach->addresses());
                punching = true;
                until = now + patience_.punch;
                show(Step::Punching);
                lucent::info("online", "punching toward the friend's {} addresses",
                             reach->addresses().size());
            }
        } else if (!replying && now - started >= patience_.direct) {
            if (gathered.reach.varying && invite_.reach.varying) {
                fail(Failure::Unreachable);
                return std::nullopt;
            }
            replying = true;
            // The direct try reached the host's NAT, so its entry there must not match this port.
            rendezvous.reset();
            gathered = {};
            socket = std::make_unique<stream::UdpSocket>(std::uint16_t{0});
            gathered = gather(stop, *socket);
            rendezvous =
                std::make_unique<Rendezvous>(*socket, Rendezvous::Role::Friend, invite_.secret);
            rendezvous->aim(invite_.reach.addresses());
            rendezvous->keepNear();
            offer(writeReply(gathered.reach, invite_.secret), gathered.reach.varying,
                  Step::Replying);
        }
        if (now >= until) {
            fail(host && !punching ? Failure::Lapsed : Failure::Unreachable);
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<LinkMessage> Meeting::await(const std::stop_token& stop, ReliableLink& link,
                                          LinkKind kind, Clock::time_point until) {
    while (!stop.stop_requested()) {
        link.pump(pumpWait);
        if (std::optional<LinkMessage> message = link.take()) {
            if (message->kind == LinkKind::Answer && kind != LinkKind::Answer) {
                fail(Failure::Refused,
                     lan::shownName(std::string_view{message->body}.substr(1), mostRefusal));
                return std::nullopt;
            }
            if (message->kind != kind) {
                fail(Failure::Dropped);
                return std::nullopt;
            }
            return message;
        }
        if (link.broken() || link.silent() || Clock::now() >= until) {
            fail(Failure::Dropped);
            return std::nullopt;
        }
    }
    return std::nullopt;
}

void Meeting::drain(const std::stop_token& stop, ReliableLink& link,
                    std::chrono::milliseconds most) {
    const auto until = Clock::now() + most;
    while (!stop.stop_requested() && Clock::now() < until && !link.broken()) {
        link.pump(pumpWait);
    }
}

void Meeting::agreeAsHost(const std::stop_token& stop, ReliableLink& link,
                          const lan::wire::NetplayAsk& ask, Agreement& agreement) {
    const auto until = Clock::now() + patience_.agreeing;
    link.send(LinkKind::Ask, lan::wire::write(ask));
    const std::optional<LinkMessage> answer = await(stop, link, LinkKind::Answer, until);
    if (!answer) {
        return;
    }
    if (!answer->body.starts_with(yes)) {
        fail(Failure::Refused,
             lan::shownName(std::string_view{answer->body}.substr(1), mostRefusal));
        return;
    }
    link.send(LinkKind::Save, save_);
    const std::optional<LinkMessage> save = await(stop, link, LinkKind::Save, until);
    if (!save) {
        return;
    }
    agreement.hostSave = std::move(save_);
    agreement.friendSave = save->body;
    agreement.delay = delayFor(link.roundTrip().value_or(0ms));
    if (rollback_) {
        agreement.ahead = aheadFor(agreement.delay);
        agreement.delay = leastDelay;
    }
    agreement.clock = ask.clock;
    const auto waited = std::chrono::system_clock::now().time_since_epoch();
    agreement.clock.epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(waited).count();
    link.send(LinkKind::Go, goBody(agreement.delay, agreement.ahead, agreement.clock));
    const auto ackUntil = Clock::now() + goAckWait;
    while (!link.flushed() && Clock::now() < ackUntil && !stop.stop_requested()) {
        link.pump(pumpWait);
    }
}

void Meeting::agreeAsFriend(const std::stop_token& stop, ReliableLink& link, Agreement& agreement) {
    auto until = Clock::now() + patience_.agreeing;
    const std::optional<LinkMessage> asked = await(stop, link, LinkKind::Ask, until);
    if (!asked) {
        return;
    }
    std::optional<lan::wire::NetplayAsk> ask = lan::wire::readNetplayAsk(asked->body);
    if (!ask) {
        fail(Failure::Dropped);
        return;
    }
    {
        const std::lock_guard lock{mutex_};
        view_.step = Step::Asking;
        view_.asked = std::move(ask);
    }
    std::optional<std::string> own;
    while (!own) {
        if (stop.stop_requested()) {
            return;
        }
        link.pump(pumpWait);
        Inbox inbox = takeInbox();
        if (inbox.declined) {
            link.send(LinkKind::Answer, std::string{no} + *inbox.declined);
            drain(stop, link, goLinger);
            fail(Failure::Refused, *inbox.declined);
            return;
        }
        own = std::move(inbox.accepted);
        if (link.broken() || link.silent() || Clock::now() >= until) {
            fail(Failure::Dropped);
            return;
        }
    }
    show(Step::Agreeing);
    until = Clock::now() + patience_.agreeing;
    link.send(LinkKind::Answer, yes);
    link.send(LinkKind::Save, *own);
    const std::optional<LinkMessage> save = await(stop, link, LinkKind::Save, until);
    if (!save) {
        return;
    }
    const std::optional<LinkMessage> go = await(stop, link, LinkKind::Go, until);
    if (!go) {
        return;
    }
    if (go->body.size() != goBytes) {
        fail(Failure::Dropped);
        return;
    }
    agreement.hostSave = save->body;
    agreement.friendSave = std::move(*own);
    agreement.delay =
        std::clamp<unsigned>(static_cast<std::uint8_t>(go->body[0]), leastDelay, mostDelay);
    agreement.ahead = std::min<unsigned>(static_cast<std::uint8_t>(go->body[1]), mostAhead);
    agreement.clock.epochMs = static_cast<std::int64_t>(fileio::littleEndian(go->body, 2, 8));
    agreement.clock.utcOffset = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(fileio::littleEndian(go->body, 10, 4)));
    drain(stop, link, goLinger);
}

void Meeting::meet(const std::stop_token& stop) {
    auto socket = std::make_unique<stream::UdpSocket>(std::uint16_t{0});
    Gathered gathered = gather(stop, *socket);
    if (gathered.reach.publics.empty() && gathered.reach.locals.empty()) {
        fail(Failure::NoNetwork);
        return;
    }
    const std::optional<Found> found = find(stop, socket, gathered);
    if (!found) {
        return;
    }
    show(Step::Agreeing);
    lucent::info("online", "connected");
    const bool host = ask_.has_value();
    const SessionKeys& keys = found->keys;
    auto agreement = std::make_unique<Agreement>();
    agreement->port = socket->port();
    agreement->peer = found->peer;
    agreement->keys = keys;
    {
        ReliableLink link{*socket, found->peer, host ? keys.linkHost : keys.linkFriend,
                          host ? keys.linkFriend : keys.linkHost};
        if (ask_) {
            agreeAsHost(stop, link, *ask_, *agreement);
        } else {
            agreeAsFriend(stop, link, *agreement);
        }
        if (stop.stop_requested()) {
            link.send(LinkKind::Answer, std::string{no} + "the other player cancelled");
            const auto until = Clock::now() + goLinger;
            while (!link.flushed() && Clock::now() < until) {
                link.pump(pumpWait);
            }
            return;
        }
        if (failed()) {
            return;
        }
    }
    socket.reset();
    agreement->lease = std::move(gathered.lease);
    const std::lock_guard lock{mutex_};
    agreement_ = std::move(agreement);
    view_.step = Step::Ready;
    view_.code.clear();
}

} // namespace opensu::online
