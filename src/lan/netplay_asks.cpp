#include "netplay_asks.hpp"

#include <utility>

#include "lucent/log.h"

namespace opensu::lan {
namespace {

constexpr std::string_view waiting = "waiting";

} // namespace

void NetplayAsks::lapse(Clock::time_point now) {
    if (pending_ && now - pending_->started > patience) {
        pending_.reset();
    }
}

bool NetplayAsks::ask(const Peer& peer, const wire::NetplayAsk& rom, Clock::time_point now,
                      std::string& refusal) {
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (pending_ && pending_->peer.id == peer.id && pending_->rom.sha256 == rom.sha256) {
        return true;
    }
    if (pending_ && pending_->verdict.verdict == waiting) {
        refusal = "another openSU's ask waits for the player";
        return false;
    }
    wire::NetplayVerdict verdict;
    verdict.verdict = waiting;
    pending_ = Pending{.peer = peer, .rom = rom, .started = now, .verdict = std::move(verdict)};
    lucent::info("lan", "{} asks to play {} together", peer.name, rom.title);
    return true;
}

std::optional<wire::NetplayVerdict>
NetplayAsks::status(std::string_view peerId, std::string_view sha256, Clock::time_point now) {
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (!pending_ || pending_->peer.id != peerId || pending_->rom.sha256 != sha256) {
        return std::nullopt;
    }
    wire::NetplayVerdict verdict = pending_->verdict;
    if (verdict.verdict != waiting) {
        pending_.reset();
    }
    return verdict;
}

std::optional<NetplayAsks::Question> NetplayAsks::question(Clock::time_point now) const {
    const std::lock_guard lock{mutex_};
    if (!pending_ || pending_->verdict.verdict != waiting || now - pending_->started > patience) {
        return std::nullopt;
    }
    return Question{.peer = pending_->peer, .rom = pending_->rom};
}

void NetplayAsks::allow(std::string_view peerId, std::uint16_t port,
                        const secrets::crypto::Bytes& salt, Clock::time_point now) {
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (pending_ && pending_->peer.id == peerId) {
        pending_->verdict.verdict = "allowed";
        pending_->verdict.port = port;
        pending_->verdict.salt = salt;
    }
}

void NetplayAsks::decline(std::string_view peerId, std::string refusal, Clock::time_point now) {
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (pending_ && pending_->peer.id == peerId) {
        pending_->verdict.verdict = "declined";
        pending_->verdict.refusal = std::move(refusal);
    }
}

void NetplayAsks::handOver(std::string_view peerId, std::string_view sha256, std::string save) {
    const std::lock_guard lock{mutex_};
    handed_ = Handed{std::string{peerId}, std::string{sha256}, std::move(save)};
}

std::optional<std::string> NetplayAsks::takeSave(std::string_view peerId, std::string_view sha256) {
    const std::lock_guard lock{mutex_};
    if (!handed_ || handed_->peerId != peerId || handed_->sha256 != sha256) {
        return std::nullopt;
    }
    std::string save = std::move(handed_->save);
    handed_.reset();
    return save;
}

} // namespace opensu::lan
