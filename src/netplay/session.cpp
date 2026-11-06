#include "session.hpp"

#include <algorithm>
#include <utility>

#include "fileio/little_endian.hpp"
#include "lucent/log.h"

namespace opensu::netplay {
namespace {

using namespace std::chrono_literals;

enum class Message : std::uint8_t {
    /// A player's slot, the first frame and how many follow, each frame's input.
    Input = 1,
    /// A frame and the sender's checksum of its state.
    Check = 2,
    /// The sender is going.
    Leave = 3,
};

constexpr std::size_t inputBytes = 10;
constexpr std::size_t inputHeader = 1 + 1 + 8 + 1;
constexpr std::size_t checkBytes = 1 + 8 + 4;
/// How long a stalled machine waits before sending its last input again.
constexpr auto resend = 8ms;
/// How many checksums wait for the other side's; it runs at most a few frames apart.
constexpr std::size_t keptChecks = 16;

void appendInput(std::string& bytes, const PlayerInput& input) {
    fileio::appendLittleEndian(bytes, input.buttons, 2);
    for (const std::int16_t axis : input.sticks) {
        fileio::appendLittleEndian(bytes, static_cast<std::uint16_t>(axis), 2);
    }
}

PlayerInput inputAt(std::string_view bytes, std::size_t at) {
    PlayerInput input;
    input.buttons = static_cast<std::uint16_t>(fileio::littleEndian(bytes, at, 2));
    for (std::size_t axis = 0; axis < input.sticks.size(); ++axis) {
        input.sticks[axis] = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(fileio::littleEndian(bytes, at + 2 + axis * 2, 2)));
    }
    return input;
}

void trim(std::map<std::uint64_t, std::uint32_t>& checks) {
    while (checks.size() > keptChecks) {
        checks.erase(checks.begin());
    }
}

} // namespace

Session::Session(Transport& transport, SessionConfig config)
    : transport_{transport}, config_{config}, players_(config.players) {
    for (Player& player : players_) {
        player.known = config_.delay;
    }
}

void Session::submit(const PlayerInput& local) {
    Player& own = players_[config_.slot];
    own.frames[own.known % ring] = local;
    ++own.known;
    sendInput();
}

void Session::sendInput() {
    const Player& own = players_[config_.slot];
    const std::uint64_t count = std::min<std::uint64_t>(own.known, repeated);
    const std::uint64_t first = own.known - count;
    std::string message;
    message.reserve(inputHeader + count * inputBytes);
    message.push_back(static_cast<char>(Message::Input));
    message.push_back(static_cast<char>(config_.slot));
    fileio::appendLittleEndian(message, first, 8);
    message.push_back(static_cast<char>(count));
    for (std::uint64_t at = first; at < own.known; ++at) {
        appendInput(message, own.frames[at % ring]);
    }
    transport_.send(message);
    sent_ = Clock::now();
}

void Session::takeInput(std::string_view body) {
    if (body.size() < inputHeader) {
        return;
    }
    const auto slot = static_cast<std::uint8_t>(body[1]);
    const std::uint64_t first = fileio::littleEndian(body, 2, 8);
    const auto count = static_cast<std::uint8_t>(body[10]);
    if (slot >= players_.size() || slot == config_.slot ||
        body.size() != inputHeader + std::size_t{count} * inputBytes) {
        return;
    }
    Player& player = players_[slot];
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint64_t at = first + index;
        if (at == player.known) {
            const PlayerInput real = inputAt(body, inputHeader + index * inputBytes);
            player.frames[at % ring] = real;
            ++player.known;
            if (at < frame_ && player.used[at % ring] != real) {
                wrong_ = std::min(wrong_.value_or(at), at);
            }
        }
    }
}

void Session::takeCheck(std::string_view body) {
    if (body.size() != checkBytes) {
        return;
    }
    const std::uint64_t frame = fileio::littleEndian(body, 1, 8);
    otherChecks_.insert_or_assign(frame,
                                  static_cast<std::uint32_t>(fileio::littleEndian(body, 9, 4)));
    trim(otherChecks_);
    compare(frame);
}

void Session::compare(std::uint64_t frame) {
    const auto own = ownChecks_.find(frame);
    const auto other = otherChecks_.find(frame);
    if (own == ownChecks_.end() || other == otherChecks_.end()) {
        return;
    }
    if (own->second != other->second) {
        lucent::error("netplay", "the games differ at frame {}: {:08x} here, {:08x} there", frame,
                      own->second, other->second);
        ended_ = SessionEnd::Desync;
        return;
    }
    ++matched_;
    otherChecks_.erase(other);
}

void Session::take(std::string_view message) {
    if (message.empty()) {
        return;
    }
    heard_ = Clock::now();
    switch (static_cast<Message>(message.front())) {
    case Message::Input:
        takeInput(message);
        break;
    case Message::Check:
        takeCheck(message);
        break;
    case Message::Leave:
        ended_ = SessionEnd::Left;
        break;
    }
}

bool Session::canRun() const {
    return std::ranges::all_of(players_, [this](const Player& player) {
        return player.known + config_.ahead > frame_;
    });
}

std::uint64_t Session::confirmed() const {
    return std::ranges::min(players_, {}, &Player::known).known;
}

std::optional<std::uint64_t> Session::takeWrong() {
    return std::exchange(wrong_, std::nullopt);
}

void Session::rewind(std::uint64_t frame) noexcept {
    frame_ = frame;
}

void Session::lockstep() noexcept {
    config_.delay += std::exchange(config_.ahead, 0U);
    for (Player& player : players_) {
        player.known = config_.delay;
    }
}

void Session::restartJoining() noexcept {
    started_ = Clock::now();
    heard_.reset();
}

void Session::end(SessionEnd why) noexcept {
    ended_ = why;
}

bool Session::ready(std::chrono::milliseconds wait) {
    while (const std::optional<std::string> message = transport_.receive(0ms)) {
        take(*message);
    }
    if (ended_ != SessionEnd::None) {
        return false;
    }
    if (canRun()) {
        stalled_ = false;
        return true;
    }
    if (!stalled_) {
        stalled_ = true;
        ++stalls_;
    }
    const Clock::time_point now = Clock::now();
    if (now - sent_ >= resend) {
        sendInput();
    }
    if (const std::optional<std::string> message = transport_.receive(wait)) {
        take(*message);
    }
    const bool over =
        heard_ ? Clock::now() - *heard_ > config_.quiet : Clock::now() - started_ > config_.joining;
    if (over && ended_ == SessionEnd::None) {
        ended_ = SessionEnd::Quiet;
    }
    return ended_ == SessionEnd::None && canRun();
}

const PlayerInput& Session::input(unsigned slot) const {
    const Player& player = players_[slot];
    const std::uint64_t latest = std::min(frame_, player.known - 1);
    return player.frames[latest % ring];
}

void Session::advance() {
    for (unsigned slot = 0; slot < players_.size(); ++slot) {
        players_[slot].used[frame_ % ring] = input(slot);
    }
    ++frame_;
}

bool Session::checkDue() const noexcept {
    return frame_ % checkInterval == 0 && frame_ != 0;
}

void Session::check(std::uint64_t frame, std::uint32_t checksum) {
    ownChecks_.insert_or_assign(frame, checksum);
    trim(ownChecks_);
    std::string message;
    message.push_back(static_cast<char>(Message::Check));
    fileio::appendLittleEndian(message, frame, 8);
    fileio::appendLittleEndian(message, checksum, 4);
    transport_.send(message);
    compare(frame);
}

void Session::leave() {
    const std::string message(1, static_cast<char>(Message::Leave));
    for (int times = 0; times < 3; ++times) {
        transport_.send(message);
    }
}

} // namespace opensu::netplay
