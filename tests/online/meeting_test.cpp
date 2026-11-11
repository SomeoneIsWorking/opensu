// Two meetings over loopback with a stand-in STUN server: a code to a session on both sides, each
// way a meeting ends without one, and the words it ends in.
#include "online/meeting.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "check.hpp"
#include "lucent/udp_socket.h"
#include "support/test_main.hpp"

namespace {

using namespace std::chrono_literals;
using namespace opensu::online;
using lucent::net::Endpoint;
using lucent::net::UdpSocket;
using lucent::p2p::CodeProblem;
using lucent::p2p::Descriptor;
using lucent::p2p::DirectReach;
using lucent::p2p::Invite;
using opensu::test::expect;
namespace crypto = lucent::p2p::crypto;

constexpr std::uint32_t loopbackIp = 0x7F000001U;

UdpSocket bound(std::uint16_t port) {
    std::string error;
    std::optional<UdpSocket> socket = UdpSocket::bind(port, error);
    if (!socket) {
        opensu::test::fail("a UDP socket");
    }
    return std::move(*socket);
}

/// Answers each Binding request with the address it came from, plus `portShift`.
class StunServer {
  public:
    explicit StunServer(std::uint16_t portShift = 0)
        : shift_{portShift}, thread_{[this](const std::stop_token& stop) {
              serve(stop);
          }} {
    }
    [[nodiscard]] std::string address() const {
        return "127.0.0.1:" + std::to_string(socket_.port());
    }

  private:
    void serve(const std::stop_token& stop) {
        std::vector<char> buffer(2048);
        while (!stop.stop_requested()) {
            Endpoint from;
            const auto length = socket_.receive(buffer, from, 10ms);
            if (!length || *length < 20) {
                continue;
            }
            std::string answer{"\x01\x01\x00\x0C", 4};
            answer.append(buffer.data() + 4, 16);
            const auto port = static_cast<std::uint16_t>((from.port + shift_) ^ 0x2112U);
            answer.append("\x00\x20\x00\x08\x00\x01", 6);
            answer.push_back(static_cast<char>(port >> 8U));
            answer.push_back(static_cast<char>(port & 0xFFU));
            const std::uint32_t ip = from.address ^ 0x2112A442U;
            for (int shift = 24; shift >= 0; shift -= 8) {
                answer.push_back(static_cast<char>((ip >> static_cast<unsigned>(shift)) & 0xFFU));
            }
            socket_.send(from, answer);
        }
    }

    std::uint16_t shift_;
    UdpSocket socket_{bound(0)};
    std::jthread thread_;
};

Network network(const std::string& stun) {
    Network network;
    network.direct.stun_servers = stun;
    network.direct.router = false;
    // Loopback stands in for a LAN interface.
    lucent::net::Ipv4Interface loopback;
    loopback.name = "lo";
    loopback.address = 0x7F000001U;
    loopback.netmask = 0xFF000000U;
    loopback.up = true;
    loopback.running = true;
    network.direct.interfaces = std::vector{loopback};
    return network;
}

Patience quick() {
    Patience patience;
    patience.direct = 300ms;
    patience.friend_tries = 3s;
    patience.punch = 1s;
    patience.invite = 3s;
    patience.agreeing = 5s;
    return patience;
}

opensu::lan::wire::NetplayAsk ask() {
    return {.console = "gba",
            .file = "link.gba",
            .title = "Link",
            .size = 1024,
            .sha256 = std::string(64, 'a'),
            .salt = crypto::random_bytes(16),
            .facts = {.coreRevision = "abc", .options = {{"a", "1"}}, .bios = {}},
            .clock = {.epochMs = 1000, .utcOffset = 7200}};
}

template <class Check> bool waitFor(Check&& check, std::chrono::milliseconds most) {
    const auto until = std::chrono::steady_clock::now() + most;
    while (std::chrono::steady_clock::now() < until) {
        if (check()) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

Meeting::Step stepOf(const Meeting& meeting) {
    return meeting.view().step;
}

std::string inviteFrom(const Meeting& host) {
    expect(waitFor(
               [&host] {
                   return stepOf(host) == Meeting::Step::Inviting;
               },
               3s),
           "the host gets a code");
    return host.view().code;
}

Invite invited(const std::string& code) {
    CodeProblem problem{};
    return opensu::test::need(lucent::p2p::read_invite(code, codePrefixes, problem),
                              "the code reads");
}

void oneCodeToASession(bool rollback) {
    const StunServer stun;
    Meeting host{network(stun.address()), quick(), ask(), "host save", rollback};
    const Invite invite = invited(inviteFrom(host));
    expect(invite.descriptor.direct && !invite.descriptor.direct->publics.empty() &&
               !invite.descriptor.direct->varying,
           "the code holds STUN's address");
    Meeting joiner{network(stun.address()), quick(), invite};
    expect(waitFor(
               [&joiner] {
                   return stepOf(joiner) == Meeting::Step::Asking;
               },
               3s),
           "the friend is asked");
    const auto asked = joiner.view().asked;
    expect(asked && asked->sha256 == std::string(64, 'a'), "with what the host offers");
    joiner.accept("friend save");
    expect(waitFor(
               [&host, &joiner] {
                   return stepOf(host) == Meeting::Step::Ready &&
                          stepOf(joiner) == Meeting::Step::Ready;
               },
               4s),
           "both are ready");
    const std::unique_ptr<Agreement> onHost = host.takeAgreement();
    const std::unique_ptr<Agreement> onFriend = joiner.takeAgreement();
    expect(onHost && onFriend, "both hold the session");
    expect(onHost->sendKey == onFriend->receiveKey && onHost->receiveKey == onFriend->sendKey &&
               onHost->sendKey != onHost->receiveKey,
           "one key each way, the same on both");
    expect(onHost->hostSave == "host save" && onFriend->hostSave == "host save" &&
               onHost->friendSave == "friend save" && onFriend->friendSave == "friend save",
           "both saves on both sides");
    expect(onHost->delay == onFriend->delay && onHost->clock == onFriend->clock,
           "the delay and clock are the host's on both");
    expect(onHost->ahead == onFriend->ahead && (onHost->ahead >= 4) == rollback,
           "a rollback host says how far to run ahead");
    expect(onHost->port != 0 && onHost->peer.port == onFriend->port, "the host dials the friend");
    const UdpSocket rebound = bound(onHost->port);
    expect(rebound.port() == onHost->port, "the port is free for the runner");
    expect(!host.takeAgreement(), "a session is taken once");
}

void declinedSaysSo() {
    const StunServer stun;
    Meeting host{network(stun.address()), quick(), ask(), {}, false};
    Meeting joiner{network(stun.address()), quick(), invited(inviteFrom(host))};
    expect(waitFor(
               [&joiner] {
                   return stepOf(joiner) == Meeting::Step::Asking;
               },
               3s),
           "asked");
    joiner.decline("it does not have this ROM");
    expect(waitFor(
               [&host] {
                   return stepOf(host) == Meeting::Step::Failed;
               },
               3s),
           "the host hears no");
    expect(host.view().failure == Meeting::Failure::Refused &&
               host.view().refusal == "it does not have this ROM",
           "and why");
    expect(host.view().code.empty(), "and the code is gone");
}

void codeLapses() {
    const StunServer stun;
    Patience patience = quick();
    patience.invite = 1s;
    Meeting host{network(stun.address()), patience, ask(), {}, false};
    expect(waitFor(
               [&host] {
                   return stepOf(host) == Meeting::Step::Failed;
               },
               4s),
           "a code nobody used ends");
    expect(host.view().failure == Meeting::Failure::Lapsed, "as lapsed");
}

/// Reach no socket answers on.
Invite deadInvite(bool varying) {
    const UdpSocket nobody = bound(0);
    Invite invite{.secret = crypto::random_bytes(lucent::p2p::kSecretBytes), .descriptor = {}};
    invite.descriptor.direct = DirectReach{
        .port = nobody.port(), .publics = {}, .locals = {loopbackIp}, .varying = varying};
    return invite;
}

void bothStrictFailsAtOnce() {
    const StunServer first;
    const StunServer second{7};
    Meeting joiner{network(first.address() + "," + second.address()), quick(), deadInvite(true)};
    const auto started = std::chrono::steady_clock::now();
    expect(waitFor(
               [&joiner] {
                   return stepOf(joiner) == Meeting::Step::Failed;
               },
               2500ms),
           "two varying NATs fail soon");
    expect(std::chrono::steady_clock::now() - started >= 300ms, "after the direct try");
    expect(joiner.view().failure == Meeting::Failure::Unreachable, "unreachable");
    expect(Meeting::textOf(joiner.view().failure).find("Tailscale") != std::string::npos,
           "and says what to do");
}

void friendGivesUp() {
    const StunServer stun;
    Meeting joiner{network(stun.address()), quick(), deadInvite(false)};
    expect(waitFor(
               [&joiner] {
                   return stepOf(joiner) == Meeting::Step::Replying;
               },
               2s),
           "the friend gets a reply code");
    expect(joiner.view().code.starts_with("OSR"), "which is one");
    expect(waitFor(
               [&joiner] {
                   return stepOf(joiner) == Meeting::Step::Failed;
               },
               4s),
           "the friend does not wait for ever");
    expect(joiner.view().failure == Meeting::Failure::Unreachable, "and says why");
}

void hostGivesUpOnPunching() {
    const StunServer stun;
    Meeting host{network(stun.address()), quick(), ask(), {}, false};
    const Invite invite = invited(inviteFrom(host));
    Descriptor dead;
    dead.direct = DirectReach{.port = 9, .publics = {}, .locals = {loopbackIp}, .varying = false};
    const crypto::Bytes other = crypto::random_bytes(lucent::p2p::kSecretBytes);
    expect(host.reply(lucent::p2p::write_reply(dead, other, codePrefixes)) ==
               CodeProblem::OtherInvite,
           "a reply to another invite is refused");
    expect(!host.reply(lucent::p2p::write_reply(dead, invite.secret, codePrefixes)),
           "the right reply is taken");
    expect(waitFor(
               [&host] {
                   return stepOf(host) == Meeting::Step::Punching;
               },
               1s),
           "both punch");
    expect(waitFor(
               [&host] {
                   return stepOf(host) == Meeting::Step::Failed;
               },
               2s),
           "and stop");
    expect(host.view().failure == Meeting::Failure::Unreachable, "unreachable");
}

void cancellingEndsItAtOnce() {
    const StunServer stun;
    const auto started = std::chrono::steady_clock::now();
    {
        Meeting host{network(stun.address()), quick(), ask(), {}, false};
        static_cast<void>(inviteFrom(host));
    }
    expect(std::chrono::steady_clock::now() - started < 1s, "going away ends a meeting");
}

void delayFollowsTheRoundTrip() {
    expect(Meeting::delayFor(0ms) == 2, "at least two frames");
    expect(Meeting::delayFor(100ms) == 4, "half of 100 ms is three frames, and one more");
    expect(Meeting::delayFor(5000ms) == 12, "at most twelve");
}

} // namespace

int main() try {
    delayFollowsTheRoundTrip();
    oneCodeToASession(false);
    oneCodeToASession(true);
    declinedSaysSo();
    codeLapses();
    bothStrictFailsAtOnce();
    friendGivesUp();
    hostGivesUpOnPunching();
    cancellingEndsItAtOnce();
    std::printf("meeting: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
