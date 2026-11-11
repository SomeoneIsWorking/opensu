// Two openSUs agree on a game over the embedded ZeroTier node alone, through a lab root at one of
// this machine's own addresses, and their runners' datagrams cross through the bridges. One node
// per process: a lab root, a host and a friend, each started through `launch::ProcessLauncher`.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "check.hpp"
#include "launch/process_launcher.hpp"
#include "lucent/network_interfaces.h"
#include "lucent/udp_socket.h"
#include "online/meeting.hpp"
#include "online/zero_tier.hpp"
#include "support/test_main.hpp"

namespace {

namespace fs = std::filesystem;
using namespace std::chrono_literals;
using namespace opensu::online;
using Clock = std::chrono::steady_clock;
using opensu::launch::ProcessLauncher;
using opensu::test::expect;
using opensu::test::fail;
using opensu::test::need;

constexpr std::uint16_t rootPort = 39993;
constexpr auto mostToMeet = 150s;

std::string readFile(const fs::path& path) {
    std::ifstream in{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

/// The text after `key ` on the first line of `path` that starts with it, once it is there.
std::optional<std::string> lineAfter(const fs::path& path, const std::string& key,
                                     std::chrono::seconds most) {
    const auto until = Clock::now() + most;
    while (Clock::now() < until) {
        const std::string text = readFile(path);
        const std::size_t at = text.find(key + " ");
        const std::size_t end = at == std::string::npos ? at : text.find('\n', at);
        if (end != std::string::npos) {
            return text.substr(at + key.size() + 1, end - at - key.size() - 1);
        }
        std::this_thread::sleep_for(100ms);
    }
    return std::nullopt;
}

template <class Check> bool waitFor(Check&& check, std::chrono::seconds most) {
    const auto until = Clock::now() + most;
    while (Clock::now() < until) {
        if (check()) {
            return true;
        }
        std::this_thread::sleep_for(50ms);
    }
    return false;
}

/// A non-loopback address of this machine, `lab0` on Linux; ZeroTier never uses loopback.
std::string labAddress() {
    std::string error;
    const auto interfaces = lucent::net::ipv4_interfaces(error);
    if (interfaces) {
        for (const lucent::net::Ipv4Interface& each : *interfaces) {
            if (each.up && each.running && !each.loopback && each.address != 0) {
                return lucent::net::format_ipv4(each.address);
            }
        }
    }
    fail("this machine has an address that is not loopback");
}

/// No direct way at all: what connects is ZeroTier.
Network zeroTierOnly(const ZeroTier& zeroTier) {
    Network network;
    network.direct.stun_servers = "";
    network.direct.router = false;
    network.direct.interfaces = std::vector<lucent::net::Ipv4Interface>{};
    network.zerotier = zeroTier.paths();
    expect(network.zerotier != nullptr, "the node started");
    return network;
}

Patience patient() {
    Patience patience;
    patience.zerotier = 90s;
    patience.friend_tries = 120s;
    return patience;
}

opensu::lan::wire::NetplayAsk ask() {
    return {.console = "gba",
            .file = "link.gba",
            .title = "Link",
            .size = 1024,
            .sha256 = std::string(64, 'a'),
            .salt = lucent::p2p::crypto::random_bytes(16),
            .facts = {.coreRevision = "abc", .options = {}, .bios = {}},
            .clock = {.epochMs = 1000, .utcOffset = 0}};
}

/// Stands in for this side's runner: plays to the bridge until it hears the other side's.
void runnersCross(const Agreement& agreement, const std::string& own, const std::string& other) {
    expect(agreement.bridge != nullptr, "a ZeroTier game keeps its path in a bridge");
    expect(agreement.port == 0 && agreement.peer.address == 0x7F000001U,
           "the runner plays to the bridge on loopback");
    std::string error;
    const std::optional<lucent::net::UdpSocket> bound = lucent::net::UdpSocket::bind(0, error);
    const lucent::net::UdpSocket& runner = need(bound, "the runner's socket");
    std::vector<char> buffer(256);
    bool heard = false;
    const auto until = Clock::now() + 30s;
    auto lingerUntil = Clock::time_point::max();
    while (Clock::now() < until && Clock::now() < lingerUntil) {
        runner.send(agreement.peer, own);
        lucent::net::Endpoint from;
        const auto length = runner.receive(buffer, from, 100ms);
        if (!heard && length && std::string{buffer.data(), *length} == other) {
            heard = true;
            // Long enough for the other side to hear this one too.
            lingerUntil = Clock::now() + 3s;
        }
    }
    expect(heard, "the other runner's datagram crossed");
    std::printf("crossed %s\n", own.c_str());
    std::fflush(stdout);
}

int runHost(const fs::path& dir) {
    ZeroTier zeroTier{dir / "host", dir / "roots"};
    zeroTier.start();
    Meeting host{zeroTierOnly(zeroTier), patient(), ask(), "host save", false};
    expect(waitFor(
               [&host] {
                   return host.view().step == Meeting::Step::Inviting;
               },
               120s),
           "the host gets a code");
    std::printf("code %s\n", host.view().code.c_str());
    std::fflush(stdout);
    expect(waitFor(
               [&host] {
                   return host.view().step == Meeting::Step::Ready;
               },
               mostToMeet),
           "the host is ready");
    const std::unique_ptr<Agreement> agreement = host.takeAgreement();
    expect(agreement != nullptr && agreement->friendSave == "friend save", "the friend's save");
    runnersCross(*agreement, "from host", "from friend");
    return 0;
}

int runFriend(const fs::path& dir, const std::string& code) {
    ZeroTier zeroTier{dir / "friend", dir / "roots"};
    zeroTier.start();
    lucent::p2p::CodeProblem problem{};
    const std::optional<lucent::p2p::Invite> read =
        lucent::p2p::read_invite(code, codePrefixes, problem);
    const lucent::p2p::Invite& invite = need(read, "the code reads");
    const std::optional<lucent::p2p::DirectReach>& direct = invite.descriptor.direct;
    expect(invite.descriptor.zerotier.has_value() &&
               (!direct || (direct->publics.empty() && direct->locals.empty())),
           "the code names the host's node and no direct address");
    Meeting joiner{zeroTierOnly(zeroTier), patient(), invite};
    expect(waitFor(
               [&joiner] {
                   return joiner.view().step == Meeting::Step::Asking;
               },
               mostToMeet),
           "the friend is asked");
    joiner.accept("friend save");
    expect(waitFor(
               [&joiner] {
                   return joiner.view().step == Meeting::Step::Ready;
               },
               60s),
           "the friend is ready");
    const std::unique_ptr<Agreement> agreement = joiner.takeAgreement();
    expect(agreement != nullptr && agreement->hostSave == "host save", "the host's save");
    runnersCross(*agreement, "from friend", "from host");
    return 0;
}

int runTest(const fs::path& self) {
    const fs::path dir = fs::path{OPENSU_TEST_SCRATCH} / "zero-tier-meeting";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const std::string endpoint = labAddress() + "/" + std::to_string(rootPort);
    std::printf("lab root at %s\n", endpoint.c_str());
    std::string failure;
    ProcessLauncher root{opensu::launch::chooseContainment()};
    expect(root.start("opensu-zt-root", OPENSU_ZERO_TIER_ROOT,
                      {(dir / "root").string(), endpoint, (dir / "roots").string()}, failure, {},
                      {}, dir / "root.log"),
           "the lab root starts");
    expect(lineAfter(dir / "root.log", "root", 20s).has_value(), "the lab root runs");
    ProcessLauncher host{opensu::launch::chooseContainment()};
    expect(host.start("opensu-zt-host", self.string(), {"host", dir.string()}, failure, {}, {},
                      dir / "host.log"),
           "the host starts");
    const std::optional<std::string> shown = lineAfter(dir / "host.log", "code", 150s);
    const std::string& code = need(shown, "the host shows a code");
    ProcessLauncher joiner{opensu::launch::chooseContainment()};
    expect(joiner.start("opensu-zt-friend", self.string(), {"friend", dir.string(), code}, failure,
                        {}, {}, dir / "friend.log"),
           "the friend starts");
    const bool hostCrossed = lineAfter(dir / "host.log", "crossed", mostToMeet + 40s).has_value();
    const bool friendCrossed = lineAfter(dir / "friend.log", "crossed", 40s).has_value();
    waitFor(
        [&host, &joiner] {
            return !host.startedRunning() && !joiner.startedRunning();
        },
        20s);
    const int hostStatus = host.exitStatus();
    const int friendStatus = joiner.exitStatus();
    host.stop();
    joiner.stop();
    root.stop();
    if (!hostCrossed || !friendCrossed || hostStatus != 0 || friendStatus != 0) {
        for (const char* log : {"root.log", "host.log", "friend.log"}) {
            std::fprintf(stderr, "--- %s\n%s\n", log, readFile(dir / log).c_str());
        }
    }
    expect(hostCrossed && friendCrossed, "both runners heard the other over ZeroTier");
    expect(hostStatus == 0 && friendStatus == 0, "both sides ended well");
    std::printf("zero tier meeting: all checks passed\n");
    return 0;
}

} // namespace

int main(int argc, char** argv) try {
    const fs::path self = fs::absolute(argv[0]);
    if (argc == 1) {
        return runTest(self);
    }
    const std::string role = argv[1];
    if (role == "host" && argc == 3) {
        return runHost(argv[2]);
    }
    if (role == "friend" && argc == 4) {
        return runFriend(argv[2], argv[3]);
    }
    fail("usage: zero_tier_meeting_test [host <dir> | friend <dir> <code>]");
} catch (...) {
    return opensu::test::escaped();
}
