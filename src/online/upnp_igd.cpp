#include "upnp_igd.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstring>
#include <format>
#include <vector>

#include "host_network.hpp"
#include "secrets/crypto.hpp"

namespace opensu::online {
namespace {

using namespace std::chrono_literals;

constexpr std::array searchedTargets{
    std::string_view{"urn:schemas-upnp-org:device:InternetGatewayDevice:2"},
    std::string_view{"urn:schemas-upnp-org:device:InternetGatewayDevice:1"},
};
constexpr std::array connectionTypes{
    std::string_view{"urn:schemas-upnp-org:service:WANIPConnection:2"},
    std::string_view{"urn:schemas-upnp-org:service:WANIPConnection:1"},
    std::string_view{"urn:schemas-upnp-org:service:WANPPPConnection:1"},
};
constexpr std::size_t mostBody = std::size_t{256} * 1024;
constexpr int conflictInMapping = 718;
/// How many other public ports to try when the one wanted is taken.
constexpr int portTries = 4;
constexpr std::uint16_t firstRandomPort = 20000;
constexpr std::uint16_t randomPorts = 40000;

std::string lowered(std::string_view text) {
    std::string low{text};
    std::ranges::transform(low, low.begin(), [](unsigned char letter) {
        return static_cast<char>(std::tolower(letter));
    });
    return low;
}

/// The text inside the first `<tag>` of `xml`, a namespace prefix on it allowed.
std::optional<std::string_view> element(std::string_view xml, std::string_view tag) {
    const std::string open = std::format("{}>", tag);
    for (std::size_t at = xml.find(open); at != std::string_view::npos;
         at = xml.find(open, at + 1)) {
        const std::size_t start = xml.rfind('<', at);
        if (start == std::string_view::npos || xml[start + 1] == '/') {
            continue;
        }
        const std::size_t text = at + open.size();
        const std::size_t end = xml.find('<', text);
        if (end == std::string_view::npos) {
            return std::nullopt;
        }
        return xml.substr(text, end - text);
    }
    return std::nullopt;
}

/// `http://host:port` of `url`, which must be plain HTTP.
std::optional<std::string> originOf(std::string_view url) {
    constexpr std::string_view scheme = "http://";
    if (!url.starts_with(scheme)) {
        return std::nullopt;
    }
    const std::size_t end = url.find('/', scheme.size());
    return std::string{url.substr(0, end)};
}

/// The address in `origin`, without its port.
std::string hostOf(std::string_view origin) {
    constexpr std::string_view scheme = "http://";
    const std::string_view host = origin.substr(scheme.size());
    return std::string{host.substr(0, host.find(':'))};
}

std::string dotted(std::uint32_t ip) {
    std::array<std::uint8_t, 4> bytes{};
    std::memcpy(bytes.data(), &ip, bytes.size());
    return std::format("{}.{}.{}.{}", bytes[0], bytes[1], bytes[2], bytes[3]);
}

std::string searchFor(std::string_view target) {
    return std::format("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n"
                       "MAN: \"ssdp:discover\"\r\nMX: 1\r\nST: {}\r\n\r\n",
                       target);
}

std::uint16_t randomPort() {
    const secrets::crypto::Bytes random = secrets::crypto::randomBytes(2);
    return static_cast<std::uint16_t>(firstRandomPort +
                                      ((random[0] << 8U) | random[1]) % randomPorts);
}

} // namespace

std::optional<std::string> ssdpLocation(std::string_view answer) {
    const std::string low = lowered(answer);
    constexpr std::string_view header = "\r\nlocation:";
    const std::size_t at = low.find(header);
    if (at == std::string::npos) {
        return std::nullopt;
    }
    std::size_t start = at + header.size();
    const std::size_t end = low.find("\r\n", start);
    if (end == std::string::npos) {
        return std::nullopt;
    }
    while (start < end && answer[start] == ' ') {
        ++start;
    }
    return std::string{answer.substr(start, end - start)};
}

std::optional<IgdService> igdServiceIn(std::string_view description, std::string_view base) {
    for (const std::string_view type : connectionTypes) {
        const std::size_t at = description.find(type);
        if (at == std::string_view::npos) {
            continue;
        }
        const std::optional<std::string_view> control =
            element(description.substr(at), "controlURL");
        if (!control || control->empty()) {
            continue;
        }
        if (control->front() == '/') {
            return IgdService{std::string{base} + std::string{*control}, std::string{type}};
        }
        if (originOf(*control) == base) {
            return IgdService{std::string{*control}, std::string{type}};
        }
        if (control->find("://") == std::string_view::npos) {
            return IgdService{std::format("{}/{}", base, *control), std::string{type}};
        }
    }
    return std::nullopt;
}

std::unique_ptr<UpnpIgd> UpnpIgd::discover(const std::stop_token& stop, stream::UdpAddress ssdp,
                                           std::uint32_t localIp, std::chrono::milliseconds wait) {
    const net::WebClient web;
    const stream::UdpSocket socket{0};
    for (const std::string_view target : searchedTargets) {
        socket.send(ssdp, searchFor(target));
    }
    std::vector<char> buffer(stream::largestDatagram);
    const auto until = std::chrono::steady_clock::now() + wait;
    while (std::chrono::steady_clock::now() < until && !stop.stop_requested()) {
        stream::UdpAddress from;
        const std::optional<std::size_t> length = socket.receive(buffer, from, 100ms);
        if (!length) {
            continue;
        }
        const std::optional<std::string> location = ssdpLocation({buffer.data(), *length});
        const std::optional<std::string> origin = location ? originOf(*location) : std::nullopt;
        if (!origin || hostOf(*origin) != dotted(from.ip)) {
            continue;
        }
        const net::WebClient::Guard guard{
            .maxBody = mostBody, .refusal = [&origin](const std::string& url) -> std::string {
                return originOf(url) == *origin ? std::string{} : "the router's URL moved away";
            }};
        const net::Result<net::WebClient::Answer> answer = web.request(stop, *location, {}, guard);
        if (!answer.ok() || answer->status != 200) {
            continue;
        }
        std::optional<IgdService> service = igdServiceIn(answer->body, *origin);
        if (service) {
            return std::unique_ptr<UpnpIgd>{
                new UpnpIgd{*origin, std::move(*service), dotted(localIp)}};
        }
    }
    return nullptr;
}

UpnpIgd::UpnpIgd(std::string origin, IgdService service, std::string localIp)
    : origin_{std::move(origin)}, service_{std::move(service)}, localIp_{std::move(localIp)} {
}

std::optional<std::string> UpnpIgd::call(std::string_view action, const std::string& arguments,
                                         int& error) const {
    const std::string body = std::format(
        "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:{0} "
        "xmlns:u=\"{1}\">{2}</u:{0}></s:Body></s:Envelope>",
        action, service_.serviceType, arguments);
    const std::vector<std::string> headers{
        "Content-Type: text/xml; charset=\"utf-8\"",
        std::format("SOAPAction: \"{}#{}\"", service_.serviceType, action)};
    const net::WebClient::Guard guard{
        .maxBody = mostBody, .refusal = [this](const std::string& url) -> std::string {
            return originOf(url) == origin_ ? std::string{} : "the router's URL moved away";
        }};
    const net::Result<net::WebClient::Answer> answer = web_.request(
        std::stop_token{}, service_.controlUrl, headers, guard,
        net::WebClient::Sent{.method = "POST", .body = body, .user = {}, .password = {}});
    if (!answer.ok()) {
        return std::nullopt;
    }
    if (answer->status == 200) {
        return answer->body;
    }
    if (const auto code = element(answer->body, "errorCode")) {
        std::from_chars(code->data(), code->data() + code->size(), error);
    }
    return std::nullopt;
}

std::optional<Mapping> UpnpIgd::open(std::uint16_t port, std::chrono::seconds lease) {
    int error = 0;
    const std::optional<std::string> address = call("GetExternalIPAddress", {}, error);
    const std::optional<std::string_view> text =
        address ? element(*address, "NewExternalIPAddress") : std::nullopt;
    const std::optional<stream::UdpAddress> external =
        text ? stream::udpAddress(std::string{*text}, 0) : std::nullopt;
    if (!external || privateAddress(external->ip)) {
        return std::nullopt;
    }
    std::uint16_t wanted = external_ != 0 ? external_ : port;
    for (int tries = 0; tries < portTries; ++tries) {
        const std::string arguments =
            std::format("<NewRemoteHost></NewRemoteHost><NewExternalPort>{}</NewExternalPort>"
                        "<NewProtocol>UDP</NewProtocol><NewInternalPort>{}</NewInternalPort>"
                        "<NewInternalClient>{}</NewInternalClient><NewEnabled>1</NewEnabled>"
                        "<NewPortMappingDescription>openSU netplay</NewPortMappingDescription>"
                        "<NewLeaseDuration>{}</NewLeaseDuration>",
                        wanted, port, localIp_, lease.count());
        error = 0;
        if (call("AddPortMapping", arguments, error)) {
            external_ = wanted;
            return Mapping{.external = {.ip = external->ip, .port = wanted}, .lease = lease};
        }
        if (error != conflictInMapping) {
            return std::nullopt;
        }
        wanted = randomPort();
    }
    return std::nullopt;
}

void UpnpIgd::close(std::uint16_t /*port*/) {
    if (external_ == 0) {
        return;
    }
    int error = 0;
    static_cast<void>(call("DeletePortMapping",
                           std::format("<NewRemoteHost></NewRemoteHost><NewExternalPort>{}"
                                       "</NewExternalPort><NewProtocol>UDP</NewProtocol>",
                                       external_),
                           error));
    external_ = 0;
}

} // namespace opensu::online
