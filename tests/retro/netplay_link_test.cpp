// A `--netplay` file read: what it says, and what it refuses.
#include "netplay_link.hpp"

#include <cstdio>
#include <string>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::retro::netplaySetupOf;
using opensu::test::expect;

constexpr std::string_view keys =
    R"("clockMs": 1793900000000, "utcOffset": -18000, "sendKey": "0011", "receiveKey": "2233")";

std::string file(const std::string& start) {
    return start + std::string{keys} + "}";
}

void readsTheFile() {
    std::string error;
    const auto setup = opensu::test::need(
        netplaySetupOf(
            file(
                R"({"slot": 1, "players": 2, "delay": 3, "port": 4000, "peer": "192.168.1.2:4001", )"),
            error),
        "a whole file is read");
    expect(setup.session.slot == 1 && setup.session.players == 2, "the slot and the players");
    expect(setup.session.delay == 3, "the delay");
    expect(setup.port == 4000, "the port");
    expect(setup.peer.has_value(), "the peer");
    expect(setup.sendKey.size() == 2 && setup.sendKey[1] == 0x11, "the key sent with");
    expect(setup.clock.epochMs == 1793900000000 && setup.clock.utcOffset == -18000, "the clock");
}

void peerIsOptional() {
    std::string error;
    const auto setup = netplaySetupOf(file(R"({"slot": 0, "players": 2, )"), error);
    expect(setup.has_value() && !setup->peer, "no peer is learned from what arrives");
}

void refuses() {
    std::string error;
    expect(!netplaySetupOf("not json", error), "not JSON");
    expect(!netplaySetupOf(file(R"({"slot": 2, "players": 2, )"), error), "a slot past the end");
    expect(!netplaySetupOf(file(R"({"slot": 0, "players": 1, )"), error), "one player");
    expect(!netplaySetupOf(file(R"({"peer": "nowhere", )"), error), "a peer with no port");
    expect(!netplaySetupOf(R"({"slot": 0, "players": 2})", error), "no clock");
    expect(!netplaySetupOf(R"({"clockMs": 1, "utcOffset": 0})", error), "no keys");
    expect(!error.empty(), "the refusal says why");
}

} // namespace

int main() try {
    readsTheFile();
    peerIsOptional();
    refuses();
    std::printf("netplay_link: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
