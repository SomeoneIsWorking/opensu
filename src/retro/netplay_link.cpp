#include "netplay_link.hpp"

#include <charconv>
#include <cstdint>
#include <span>

#include <libretro.h>
#include <nlohmann/json.hpp>

#include "core.hpp"
#include "lucent/log.h"
#include "secrets/secret.hpp"
#include "zip/zip_archive.hpp"

namespace opensu::retro {
namespace {

using json = nlohmann::json;

/// Input messages repeat 16 frames, and a guess must be put right before it leaves them.
constexpr unsigned mostAhead = 12;

std::optional<std::vector<std::uint8_t>> keyOf(const json& document, const char* name) {
    const auto found = document.find(name);
    if (found == document.end() || !found->is_string()) {
        return std::nullopt;
    }
    return secrets::hexBytes(found->get<std::string>());
}

std::optional<stream::UdpAddress> addressOf(const std::string& text) {
    const std::size_t colon = text.rfind(':');
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    std::uint16_t port = 0;
    const char* end = text.data() + text.size();
    const auto [parsed, failure] = std::from_chars(text.data() + colon + 1, end, port);
    if (failure != std::errc{} || parsed != end) {
        return std::nullopt;
    }
    return stream::udpAddress(text.substr(0, colon), port);
}

netplay::PlayerInput inputOf(const Joypad& joypad) {
    netplay::PlayerInput input;
    // A session's machine plays one port, whatever its pads: everything here is port 0.
    input.buttons = static_cast<std::uint16_t>(joypad.state(0, RETRO_DEVICE_ID_JOYPAD_MASK));
    input.sticks = {
        joypad.analog(0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X),
        joypad.analog(0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y),
        joypad.analog(0, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X),
        joypad.analog(0, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y),
    };
    return input;
}

/// The system RAM's CRC-32, or the state's on a core that shows no RAM.
std::uint32_t checksumOf(Core& core) {
    const std::span<const std::byte> memory = core.systemRam();
    if (!memory.empty()) {
        return zip::crc32Of({reinterpret_cast<const char*>(memory.data()), memory.size()});
    }
    const std::vector<std::byte> state = core.serialize();
    return zip::crc32Of({reinterpret_cast<const char*>(state.data()), state.size()});
}

} // namespace

std::optional<NetplaySetup> netplaySetupOf(const std::string& text, std::string& error) {
    const json document = json::parse(text, nullptr, false);
    if (!document.is_object()) {
        error = "the netplay file is not a JSON object";
        return std::nullopt;
    }
    NetplaySetup setup;
    setup.session.slot = document.value("slot", 0U);
    setup.session.players = document.value("players", 2U);
    setup.session.delay = document.value("delay", setup.session.delay);
    setup.session.ahead = document.value("ahead", 0U);
    setup.port = document.value("port", std::uint16_t{0});
    if (setup.session.players < 2 || setup.session.slot >= setup.session.players ||
        setup.session.players > maxPlayers) {
        error = "the netplay file's slot and players do not fit";
        return std::nullopt;
    }
    if (setup.session.ahead > mostAhead) {
        error = "the netplay file's ahead is over what input messages repeat";
        return std::nullopt;
    }
    if (const auto peer = document.find("peer"); peer != document.end()) {
        setup.peer = peer->is_string() ? addressOf(peer->get<std::string>()) : std::nullopt;
        if (!setup.peer) {
            error = "the netplay file's peer is no address and port";
            return std::nullopt;
        }
    }
    const auto clockMs = document.find("clockMs");
    const auto utcOffset = document.find("utcOffset");
    if (clockMs == document.end() || !clockMs->is_number_integer() || utcOffset == document.end() ||
        !utcOffset->is_number_integer()) {
        error = "the netplay file lacks its clock";
        return std::nullopt;
    }
    setup.clock = {.epochMs = clockMs->get<std::int64_t>(),
                   .utcOffset = utcOffset->get<std::int32_t>()};
    std::optional<std::vector<std::uint8_t>> send = keyOf(document, "sendKey");
    std::optional<std::vector<std::uint8_t>> receive = keyOf(document, "receiveKey");
    if (!send || !receive) {
        error = "the netplay file lacks its keys";
        return std::nullopt;
    }
    setup.sendKey = std::move(*send);
    setup.receiveKey = std::move(*receive);
    return setup;
}

NetplayLink::NetplayLink(const NetplaySetup& setup, const Joypad& joypad)
    : players_{setup.session.players}, ahead_{setup.session.ahead},
      transport_{setup.port, setup.peer, setup.sendKey, setup.receiveKey},
      session_{transport_, setup.session}, joypad_{joypad} {
}

void NetplayLink::attach(Core& core, const CoreEnvironment& environment) {
    const std::size_t size = core.serializeSize();
    if (ahead_ == 0 || size == 0 || !environment.canRollBack()) {
        session_.lockstep();
        lucent::info("retro", "netplay: lockstep");
        return;
    }
    firstState_ = core.serialize();
    if (firstState_.empty()) {
        session_.lockstep();
        lucent::info("retro", "netplay: lockstep, as the core gave no state");
        return;
    }
    states_.assign(ahead_ + 2, std::vector<std::byte>(size));
    sync_ = std::make_unique<netplay::StateSync>(
        transport_,
        session_.slot() == 0 ? netplay::StateSync::Role::Offers : netplay::StateSync::Role::Takes,
        firstState_);
    lucent::info("retro", "netplay: rollback up to {} frames, states of {} bytes", ahead_, size);
}

bool NetplayLink::started(Core& core, std::chrono::milliseconds wait) {
    if (!sync_) {
        return true;
    }
    using Outcome = netplay::StateSync::Outcome;
    const Outcome outcome = sync_->pump(wait);
    if (outcome == Outcome::Running) {
        return false;
    }
    if (outcome == Outcome::Lapsed) {
        session_.end(netplay::SessionEnd::Quiet);
    } else if (outcome == Outcome::Refused ||
               (outcome == Outcome::Received && !core.unserialize(sync_->state()))) {
        session_.end(netplay::SessionEnd::Desync);
    }
    lucent::info("retro", "netplay: first state {}",
                 outcome == Outcome::Received ? "taken from player 1" : "settled");
    sync_.reset();
    firstState_.clear();
    session_.restartJoining();
    return session_.ended() == netplay::SessionEnd::None;
}

void NetplayLink::keepState(Core& core) {
    std::vector<std::byte>& state = states_[session_.frame() % states_.size()];
    if (core.serializeSize() != state.size() || !core.serializeInto(state)) {
        lucent::error("retro", "the core gave no state for frame {}", session_.frame());
        session_.end(netplay::SessionEnd::Desync);
    }
}

void NetplayLink::rollBack(Core& core, std::uint64_t frame) {
    const std::uint64_t target = session_.frame();
    if (!core.unserialize(states_[frame % states_.size()])) {
        lucent::error("retro", "the core refused its own state of frame {}", frame);
        session_.end(netplay::SessionEnd::Desync);
        return;
    }
    session_.rewind(frame);
    while (session_.frame() < target && session_.ended() == netplay::SessionEnd::None) {
        keepState(core);
        core.runQuiet();
        finishFrame(core);
        ++replayed_;
    }
}

void NetplayLink::sendChecks() {
    const std::uint64_t confirmed = session_.confirmed();
    for (auto check = checks_.begin(); check != checks_.end() && check->first < confirmed;) {
        session_.check(check->first, check->second);
        check = checks_.erase(check);
    }
}

bool NetplayLink::ready(Core& core, std::chrono::milliseconds wait) {
    if (!started(core, wait)) {
        return false;
    }
    if (!submitted_) {
        session_.submit(inputOf(joypad_));
        submitted_ = true;
    }
    const bool ready = session_.ready(wait);
    if (const std::optional<std::uint64_t> wrong = session_.takeWrong()) {
        rollBack(core, *wrong);
    }
    sendChecks();
    if (ready && rollingBack()) {
        keepState(core);
    }
    return ready && session_.ended() == netplay::SessionEnd::None;
}

void NetplayLink::finishFrame(Core& core) {
    if (session_.checkDue()) {
        checks_.insert_or_assign(session_.frame(), checksumOf(core));
    }
    session_.advance();
}

void NetplayLink::ran(Core& core) {
    finishFrame(core);
    submitted_ = false;
}

void NetplayLink::leave() {
    if (session_.ended() == netplay::SessionEnd::None) {
        session_.leave();
    }
}

std::int16_t NetplayLink::state(unsigned port, unsigned id) const {
    const std::uint16_t bits = port < playerCount() ? session_.input(port).buttons : 0;
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
        return static_cast<std::int16_t>(bits);
    }
    return id < 16 ? static_cast<std::int16_t>((bits >> id) & 1U) : std::int16_t{0};
}

std::int16_t NetplayLink::analog(unsigned port, unsigned index, unsigned id) const {
    if (port >= playerCount() || index > RETRO_DEVICE_INDEX_ANALOG_RIGHT ||
        id > RETRO_DEVICE_ID_ANALOG_Y) {
        return 0;
    }
    const std::size_t axis = (index == RETRO_DEVICE_INDEX_ANALOG_LEFT ? 0U : 2U) +
                             (id == RETRO_DEVICE_ID_ANALOG_X ? 0U : 1U);
    return session_.input(port).sticks[axis];
}

} // namespace opensu::retro
