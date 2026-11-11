#include "meeting.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "fileio/little_endian.hpp"
#include "lan/peer_book.hpp"
#include "lucent/log.h"

namespace opensu::online {
namespace {

using namespace std::chrono_literals;
using lucent::p2p::Connector;
using lucent::p2p::Role;

constexpr auto pumpWait = 20ms;
constexpr auto watchWait = 20ms;
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
/// The purpose the runners' session keys are derived for.
constexpr std::string_view gamePurpose = "game";

lucent::p2p::LinkCaps linkCaps() {
    lucent::p2p::LinkCaps caps{};
    caps[static_cast<std::uint8_t>(LinkKind::Ask)] = std::size_t{64} * 1024;
    caps[static_cast<std::uint8_t>(LinkKind::Answer)] = 512;
    caps[static_cast<std::uint8_t>(LinkKind::Save)] = std::size_t{4} * 1024 * 1024;
    caps[static_cast<std::uint8_t>(LinkKind::Go)] = 32;
    return caps;
}

std::uint8_t byteOf(LinkKind kind) {
    return static_cast<std::uint8_t>(kind);
}

std::string goBody(unsigned delay, unsigned ahead, const lan::wire::NetplayClock& clock) {
    std::string body(1, static_cast<char>(delay));
    body.push_back(static_cast<char>(ahead));
    fileio::appendLittleEndian(body, static_cast<std::uint64_t>(clock.epochMs), 8);
    fileio::appendLittleEndian(body, static_cast<std::uint32_t>(clock.utcOffset), 4);
    return body;
}

Meeting::Step stepOf(Connector::Step step) {
    switch (step) {
    case Connector::Step::Gathering:
        return Meeting::Step::Gathering;
    case Connector::Step::Inviting:
        return Meeting::Step::Inviting;
    case Connector::Step::Trying:
        return Meeting::Step::Trying;
    case Connector::Step::Replying:
        return Meeting::Step::Replying;
    case Connector::Step::Punching:
        return Meeting::Step::Punching;
    case Connector::Step::Connected:
        return Meeting::Step::Agreeing;
    case Connector::Step::Failed:
        return Meeting::Step::Failed;
    }
    return Meeting::Step::Failed;
}

Meeting::Failure failureOf(Connector::Failure failure) {
    switch (failure) {
    case Connector::Failure::None:
        return Meeting::Failure::None;
    case Connector::Failure::Unreachable:
        return Meeting::Failure::Unreachable;
    case Connector::Failure::Lapsed:
        return Meeting::Failure::Lapsed;
    case Connector::Failure::NoNetwork:
        return Meeting::Failure::NoNetwork;
    }
    return Meeting::Failure::NoNetwork;
}

} // namespace

std::string_view textOf(lucent::p2p::CodeProblem problem) noexcept {
    using lucent::p2p::CodeProblem;
    switch (problem) {
    case CodeProblem::Empty:
        return "Paste the code first";
    case CodeProblem::Unknown:
        return "That is not an openSU code";
    case CodeProblem::WrongKind:
        return "That is the other kind of code: an invite goes in Join, a reply on the host's page";
    case CodeProblem::Damaged:
        return "That code is damaged: copy it again, whole";
    case CodeProblem::Version:
        return "That code is from another version of openSU: update both to the same one";
    case CodeProblem::OtherInvite:
        return "That reply is to another invite";
    }
    return {};
}

lucent::p2p::ConnectorConfig Meeting::configOf(Network network, const Patience& patience) {
    return {.direct = std::move(network),
            .patience = patience,
            .prefixes = codePrefixes,
            .label = std::string{keyLabel}};
}

Meeting::Meeting(Network network, Patience patience, lan::wire::NetplayAsk ask, std::string save,
                 bool rollback)
    : patience_{patience}, ask_{std::move(ask)}, save_{std::move(save)}, rollback_{rollback},
      connector_{std::make_unique<Connector>(configOf(std::move(network), patience))},
      worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

Meeting::Meeting(Network network, Patience patience, lucent::p2p::Invite invite)
    : patience_{patience}, connector_{std::make_unique<Connector>(
                               configOf(std::move(network), patience), std::move(invite))},
      worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

Meeting::~Meeting() {
    worker_.request_stop();
    worker_.join();
}

Meeting::View Meeting::view() const {
    const std::lock_guard lock{mutex_};
    return view_;
}

std::optional<lucent::p2p::CodeProblem> Meeting::reply(std::string_view code) {
    return connector_->reply(code);
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

std::unique_ptr<lucent::p2p::Connection> Meeting::connect(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        const Connector::View seen = connector_->view();
        {
            const std::lock_guard lock{mutex_};
            view_.step = stepOf(seen.step);
            view_.code = seen.code;
            view_.closed = seen.closed;
        }
        if (seen.step == Connector::Step::Failed) {
            fail(failureOf(seen.failure));
            return nullptr;
        }
        if (seen.step == Connector::Step::Connected) {
            return connector_->take();
        }
        std::this_thread::sleep_for(watchWait);
    }
    return nullptr;
}

std::optional<lucent::p2p::LinkMessage> Meeting::await(const std::stop_token& stop,
                                                       ReliableLink& link, LinkKind kind,
                                                       Clock::time_point until) {
    while (!stop.stop_requested()) {
        link.pump(pumpWait);
        if (std::optional<LinkMessage> message = link.take()) {
            if (message->kind == byteOf(LinkKind::Answer) && kind != LinkKind::Answer) {
                fail(Failure::Refused,
                     lan::shownName(std::string_view{message->body}.substr(1), mostRefusal));
                return std::nullopt;
            }
            if (message->kind != byteOf(kind)) {
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
    link.send(byteOf(LinkKind::Ask), lan::wire::write(ask));
    const std::optional<LinkMessage> answer = await(stop, link, LinkKind::Answer, until);
    if (!answer) {
        return;
    }
    if (!answer->body.starts_with(yes)) {
        fail(Failure::Refused,
             lan::shownName(std::string_view{answer->body}.substr(1), mostRefusal));
        return;
    }
    link.send(byteOf(LinkKind::Save), save_);
    const std::optional<LinkMessage> save = await(stop, link, LinkKind::Save, until);
    if (!save) {
        return;
    }
    agreement.hostSave = std::move(save_);
    agreement.friendSave = save->body;
    agreement.delay = delayFor(link.round_trip().value_or(0ms));
    if (rollback_) {
        agreement.ahead = aheadFor(agreement.delay);
        agreement.delay = leastDelay;
    }
    agreement.clock = ask.clock;
    const auto waited = std::chrono::system_clock::now().time_since_epoch();
    agreement.clock.epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(waited).count();
    link.send(byteOf(LinkKind::Go), goBody(agreement.delay, agreement.ahead, agreement.clock));
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
            link.send(byteOf(LinkKind::Answer), std::string{no} + *inbox.declined);
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
    link.send(byteOf(LinkKind::Answer), yes);
    link.send(byteOf(LinkKind::Save), *own);
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
    std::unique_ptr<lucent::p2p::Connection> connection = connect(stop);
    if (!connection) {
        return;
    }
    show(Step::Agreeing);
    lucent::info("online", "connected");
    const bool host = ask_.has_value();
    const Role own = host ? Role::Host : Role::Friend;
    const Role other = host ? Role::Friend : Role::Host;
    const lucent::p2p::SessionKeys& keys = connection->keys;
    auto agreement = std::make_unique<Agreement>();
    agreement->port = connection->path->port();
    agreement->sendKey = keys.key(gamePurpose, own);
    agreement->receiveKey = keys.key(gamePurpose, other);
    {
        const lucent::p2p::crypto::Bytes linkSend = keys.key(lucent::p2p::kLinkPurpose, own);
        const lucent::p2p::crypto::Bytes linkReceive = keys.key(lucent::p2p::kLinkPurpose, other);
        ReliableLink link{*connection->path, connection->peer, {linkSend, linkReceive}, linkCaps()};
        if (ask_) {
            agreeAsHost(stop, link, *ask_, *agreement);
        } else {
            agreeAsFriend(stop, link, *agreement);
        }
        if (stop.stop_requested()) {
            link.send(byteOf(LinkKind::Answer), std::string{no} + "the other player cancelled");
            const auto until = Clock::now() + goLinger;
            while (!link.flushed() && Clock::now() < until) {
                link.pump(pumpWait);
            }
            return;
        }
        if (failed()) {
            return;
        }
        const std::optional<lucent::net::Endpoint> peer = link.peer().endpoint();
        if (!peer) {
            fail(Failure::Dropped);
            return;
        }
        agreement->peer = *peer;
    }
    connection->path.reset();
    agreement->lease = std::move(connection->lease);
    const std::lock_guard lock{mutex_};
    agreement_ = std::move(agreement);
    view_.step = Step::Ready;
    view_.code.clear();
}

} // namespace opensu::online
