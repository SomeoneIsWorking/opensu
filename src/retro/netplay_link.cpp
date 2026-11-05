#include "netplay_link.hpp"

#include <charconv>
#include <cstdint>
#include <span>

#include <libretro.h>
#include <nlohmann/json.hpp>

#include "core.hpp"
#include "secrets/secret.hpp"
#include "zip/zip_archive.hpp"

namespace opensu::retro {
namespace {

using json = nlohmann::json;

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
    setup.port = document.value("port", std::uint16_t{0});
    if (setup.session.players < 2 || setup.session.slot >= setup.session.players ||
        setup.session.players > maxPlayers) {
        error = "the netplay file's slot and players do not fit";
        return std::nullopt;
    }
    if (const auto peer = document.find("peer"); peer != document.end()) {
        setup.peer = peer->is_string() ? addressOf(peer->get<std::string>()) : std::nullopt;
        if (!setup.peer) {
            error = "the netplay file's peer is no address and port";
            return std::nullopt;
        }
    }
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
    : players_{setup.session.players},
      transport_{setup.port, setup.peer, setup.sendKey, setup.receiveKey},
      session_{transport_, setup.session}, joypad_{joypad} {
}

bool NetplayLink::ready(std::chrono::milliseconds wait) {
    if (!submitted_) {
        session_.submit(inputOf(joypad_));
        submitted_ = true;
    }
    return session_.ready(wait);
}

void NetplayLink::ran(Core& core) {
    if (session_.checkDue()) {
        const std::span<const std::byte> memory = core.systemRam();
        session_.check(zip::crc32Of({reinterpret_cast<const char*>(memory.data()), memory.size()}));
    }
    session_.advance();
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
