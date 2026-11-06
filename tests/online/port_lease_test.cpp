// A router's mapping through the shipping NAT-PMP and UPnP clients against loopback stand-ins: it
// is renewed, taken down when the lease goes, and refused when the router will not keep it short.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "check.hpp"
#include "online/nat_pmp.hpp"
#include "online/port_lease.hpp"
#include "online/upnp_igd.hpp"
#include "support/loopback_server.hpp"
#include "support/test_main.hpp"

namespace {

using namespace std::chrono_literals;
using namespace opensu::online;
using opensu::stream::UdpAddress;
using opensu::stream::UdpSocket;
using opensu::test::expect;
using opensu::test::need;

UdpAddress loopback(std::uint16_t port) {
    return need(opensu::stream::udpAddress("127.0.0.1", port), "loopback");
}

std::uint16_t word(std::string_view bytes, std::size_t at) {
    return static_cast<std::uint16_t>((static_cast<std::uint8_t>(bytes[at]) << 8U) |
                                      static_cast<std::uint8_t>(bytes[at + 1]));
}

/// A NAT-PMP gateway granting `granted` seconds, its public address 203.0.113.9.
class Gateway {
  public:
    explicit Gateway(std::uint32_t granted)
        : granted_{granted}, thread_{[this](const std::stop_token& stop) {
              serve(stop);
          }} {
    }
    [[nodiscard]] UdpAddress address() const {
        return loopback(socket_.port());
    }
    [[nodiscard]] int maps() const {
        return maps_;
    }
    [[nodiscard]] int removals() const {
        return removals_;
    }

  private:
    void serve(const std::stop_token& stop) {
        std::vector<char> buffer(64);
        while (!stop.stop_requested()) {
            UdpAddress from;
            const auto length = socket_.receive(buffer, from, 20ms);
            if (!length || *length < 2) {
                continue;
            }
            std::string answer(2, '\0');
            answer[1] = static_cast<char>(buffer[1] | 0x80);
            answer.append(6, '\0');
            if (buffer[1] == 0) {
                answer.append("\xCB\x00\x71\x09", 4);
            } else {
                const std::string_view request{buffer.data(), *length};
                const std::uint32_t asked =
                    (std::uint32_t{word(request, 8)} << 16U) | word(request, 10);
                asked == 0 ? ++removals_ : ++maps_;
                answer.append(request.substr(4, 2));
                answer.append(request.substr(4, 2));
                const std::uint32_t lifetime = asked == 0 ? 0 : granted_;
                for (int shift = 24; shift >= 0; shift -= 8) {
                    answer.push_back(static_cast<char>((lifetime >> shift) & 0xFFU));
                }
            }
            socket_.send(from, answer);
        }
    }

    std::uint32_t granted_;
    UdpSocket socket_{0};
    std::atomic<int> maps_{0};
    std::atomic<int> removals_{0};
    std::jthread thread_;
};

void leaseIsKeptAndTakenDown() {
    const Gateway gateway{2};
    {
        auto mapper = std::make_unique<NatPmp>(gateway.address());
        const std::optional<Mapping> mapping = mapper->open(4000, askedLease);
        const Mapping& granted = need(mapping, "the gateway grants a lease");
        expect(granted.lease == 2s, "the lease is short");
        expect(granted.external ==
                   need(opensu::stream::udpAddress("203.0.113.9", 4000), "an address"),
               "the mapping is at the gateway's public address");
        const PortLease lease{std::move(mapper), 4000, granted};
        std::this_thread::sleep_for(2500ms);
        expect(gateway.maps() >= 3, "the lease is renewed at half its time");
        expect(gateway.removals() == 0, "it stays up meanwhile");
    }
    expect(gateway.removals() == 1, "the mapping goes with the lease");
}

void longLeaseRefused() {
    const Gateway gateway{86400};
    NatPmp mapper{gateway.address()};
    expect(!mapper.open(4000, askedLease), "a day's lease is not kept");
    expect(gateway.removals() == 1, "and what the gateway made is taken down");
}

void noGateway() {
    const UdpSocket silent{0};
    NatPmp mapper{loopback(silent.port())};
    const auto started = std::chrono::steady_clock::now();
    expect(!mapper.open(4000, askedLease), "no gateway, no mapping");
    expect(std::chrono::steady_clock::now() - started < 5s, "and it gives up soon");
}

/// A router answering SSDP on one socket and SOAP on a loopback server.
struct Router {
    std::mutex mutex;
    std::vector<std::string> actions;
    /// The UPnP error AddPortMapping answers with, 0 for none.
    int refuseWith{0};
    std::string externalIp{"203.0.113.9"};
    opensu::test::LoopbackServer soap{[this](const lucent::http::Request& request) {
        if (request.method == "GET") {
            return opensu::test::reply(
                200, "<root><device><serviceList><service><serviceType>"
                     "urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
                     "<controlURL>/ctl</controlURL></service></serviceList></device></root>");
        }
        const std::lock_guard lock{mutex};
        if (request.body.find("GetExternalIPAddress") != std::string::npos) {
            actions.emplace_back("external");
            return opensu::test::reply(200, "<u:R><NewExternalIPAddress>" + externalIp +
                                                "</NewExternalIPAddress></u:R>");
        }
        if (request.body.find("DeletePortMapping") != std::string::npos) {
            actions.emplace_back("delete");
            return opensu::test::reply(200, "<ok/>");
        }
        actions.emplace_back(request.body.find("<NewLeaseDuration>120<") != std::string::npos
                                 ? "add 120"
                                 : "add other");
        if (refuseWith != 0) {
            return opensu::test::reply(500, "<UPnPError><errorCode>" + std::to_string(refuseWith) +
                                                "</errorCode></UPnPError>");
        }
        return opensu::test::reply(200, "<ok/>");
    }};
    UdpSocket ssdp{0};
    std::string location{soap.base() + "/desc.xml"};
    std::jthread answering{[this](const std::stop_token& stop) {
        std::vector<char> buffer(2048);
        while (!stop.stop_requested()) {
            UdpAddress from;
            if (ssdp.receive(buffer, from, 20ms)) {
                ssdp.send(from, "HTTP/1.1 200 OK\r\nLOCATION: " + location + "\r\n\r\n");
            }
        }
    }};

    [[nodiscard]] bool did(const std::string& action) {
        const std::lock_guard lock{mutex};
        return std::ranges::find(actions, action) != actions.end();
    }
};

std::unique_ptr<UpnpIgd> found(Router& router) {
    return UpnpIgd::discover({}, loopback(router.ssdp.port()), 0x0100007FU, 500ms);
}

void upnpMapsAndDeletes() {
    Router router;
    auto igd = found(router);
    expect(igd != nullptr, "the router is found");
    const std::optional<Mapping> mapping = igd->open(4000, askedLease);
    expect(mapping && mapping->external.port == 4000, "the port is mapped");
    expect(router.did("add 120"), "with a short lease");
    igd->close(4000);
    expect(router.did("delete"), "and taken down");
}

void upnpPermanentOnlyRefused() {
    Router router;
    router.refuseWith = 725;
    auto igd = found(router);
    expect(igd != nullptr, "the router is found");
    expect(!igd->open(4000, askedLease), "a router offering only permanent mappings gets none");
    expect(!router.did("add other"), "no mapping without a lease is asked for");
}

void upnpDoubleNatRefused() {
    Router router;
    router.externalIp = "192.168.1.9";
    auto igd = found(router);
    expect(igd != nullptr && !igd->open(4000, askedLease), "a private WAN address maps nothing");
    expect(!router.did("add 120"), "and nothing is opened on it");
}

void upnpOtherHostRefused() {
    Router router;
    router.location = "http://198.51.100.7:80/desc.xml";
    expect(found(router) == nullptr, "a LOCATION off the answering address is not followed");
}

void descriptionParsed() {
    const std::string base = "http://192.168.1.1:5000";
    const auto service = igdServiceIn("<serviceType>urn:schemas-upnp-org:service:WANPPPConnection:1"
                                      "</serviceType><controlURL>ctl/IPConn</controlURL>",
                                      base);
    expect(service && service->controlUrl == base + "/ctl/IPConn", "a relative URL is resolved");
    expect(!igdServiceIn("<serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
                         "<controlURL>http://203.0.113.1/ctl</controlURL>",
                         base),
           "a control URL on another host is refused");
}

} // namespace

int main() try {
    leaseIsKeptAndTakenDown();
    longLeaseRefused();
    noGateway();
    descriptionParsed();
    upnpMapsAndDeletes();
    upnpPermanentOnlyRefused();
    upnpDoubleNatRefused();
    upnpOtherHostRefused();
    std::printf("port lease: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
