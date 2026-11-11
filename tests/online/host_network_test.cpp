// Which interfaces' addresses a code keeps, in what order: Tailscale's and ZeroTier's before the
// LAN's, idle bridges and link-local addresses left out.
#include "online/host_network.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <optional>
#include <vector>

#include "check.hpp"
#include "stream/udp_socket.hpp"
#include "support/test_main.hpp"

namespace {

using namespace opensu::online;
using opensu::test::expect;

using lucent::net::Ipv4Interface;

std::uint32_t ip(const char* dotted) {
    return opensu::stream::udpAddress(dotted, 0)->ip;
}

std::uint32_t hostOrder(const char* dotted) {
    const std::uint32_t raw = ip(dotted);
    std::array<std::uint8_t, 4> bytes{};
    std::memcpy(bytes.data(), &raw, bytes.size());
    return (std::uint32_t{bytes[0]} << 24U) | (std::uint32_t{bytes[1]} << 16U) |
           (std::uint32_t{bytes[2]} << 8U) | bytes[3];
}

Ipv4Interface named(const char* address, const char* mask, const char* name, bool running = true) {
    Ipv4Interface interface;
    interface.name = name;
    interface.address = hostOrder(address);
    interface.netmask = hostOrder(mask);
    interface.up = true;
    interface.running = running;
    return interface;
}

void zeroTierIsAFallback() {
    std::vector<Ipv4Interface> found{
        named("192.168.1.107", "255.255.255.0", "enp8s0"),
        named("172.17.0.1", "255.255.0.0", "docker0", false),
        named("192.168.122.1", "255.255.255.0", "virbr0", false),
        named("10.88.0.1", "255.255.0.0", "podman0"),
        named("169.254.3.4", "255.255.0.0", "enp9s0"),
        named("10.147.17.5", "255.255.255.0", "zt3jn4abcd"),
        named("127.0.0.1", "255.0.0.0", "lo"),
    };
    found.back().loopback = true;
    const std::vector<std::uint32_t> kept = reachableLocals(found, ip("192.168.1.1"));
    const std::vector<std::uint32_t> expected{ip("10.147.17.5"), ip("192.168.1.107"),
                                              ip("10.88.0.1")};
    expect(kept == expected, "ZeroTier first, then the gateway's; idle bridges, link-local and loopback out");
}

void tailscaleAndZeroTierShareTheFront() {
    const std::vector<Ipv4Interface> found{
        named("10.0.1.2", "255.255.255.0", "eth0"),
        named("100.101.102.103", "255.192.0.0", "tailscale0"),
        named("10.147.17.5", "255.255.255.0", "zt0"),
    };
    const std::vector<std::uint32_t> kept = reachableLocals(found, ip("10.0.1.1"));
    const std::vector<std::uint32_t> expected{ip("100.101.102.103"), ip("10.147.17.5"),
                                              ip("10.0.1.2")};
    expect(kept == expected, "the overlays keep their order before the LAN");
}

void zeroTierByItsDriver() {
    Ipv4Interface windows = named("10.147.17.5", "255.255.255.0", "Ethernet 3");
    windows.description = "ZeroTier Virtual Port";
    expect(zeroTier(windows), "Windows' adapter, renamed, by its description");
    expect(zeroTier(named("10.147.17.5", "255.255.255.0", "feth2913")), "macOS' feth");
    expect(!zeroTier(named("10.147.17.5", "255.255.255.0", "corpnet")),
           "a renamed Linux interface is ranked as any other");
    const std::vector<Ipv4Interface> renamed{named("10.0.1.2", "255.255.255.0", "eth0"),
                                         named("10.147.17.5", "255.255.255.0", "corpnet")};
    expect(reachableLocals(renamed, ip("10.0.1.1")).size() == 2, "and still kept");
}

} // namespace

int main() try {
    zeroTierIsAFallback();
    tailscaleAndZeroTierShareTheFront();
    zeroTierByItsDriver();
    std::printf("host network: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
