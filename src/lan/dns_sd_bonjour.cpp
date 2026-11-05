#include "dns_sd.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>

#include <arpa/inet.h>
#include <dns_sd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>

#include <SystemConfiguration/SystemConfiguration.h>

#include "lucent/log.h"
#include "peer_book.hpp"

namespace opensu::lan {
namespace {

struct RefRelease {
    void operator()(DNSServiceRef ref) const noexcept {
        DNSServiceRefDeallocate(ref);
    }
};
/// A DNS-SD operation, or the connection they share; ended with its owner.
using Ref = std::unique_ptr<std::remove_pointer_t<DNSServiceRef>, RefRelease>;

/// What mDNSResponder's error `code` means, for a log line or the player.
std::string errorText(DNSServiceErrorType code) {
    switch (code) {
    case kDNSServiceErr_ServiceNotRunning:
        return "mDNSResponder is not running";
    case kDNSServiceErr_PolicyDenied:
        return "macOS does not let openSU use the local network; allow it in System Settings › "
               "Privacy & Security › Local Network";
    case kDNSServiceErr_NoAuth:
        return "macOS does not allow openSU to announce itself";
    default:
        return "mDNSResponder error " + std::to_string(code);
    }
}

/// The TXT entry `key` of a record, empty when it has none.
std::string txtValue(std::uint16_t length, const unsigned char* record, const char* key) {
    std::uint8_t size = 0;
    const void* value = TXTRecordGetValuePtr(length, record, key, &size);
    return value == nullptr ? std::string{} : std::string{static_cast<const char*>(value), size};
}

void DNSSD_API registered(DNSServiceRef ref, DNSServiceFlags flags, DNSServiceErrorType code,
                          const char* name, const char* type, const char* domain, void* context);
void DNSSD_API browsed(DNSServiceRef ref, DNSServiceFlags flags, std::uint32_t interface,
                       DNSServiceErrorType code, const char* name, const char* type,
                       const char* domain, void* context);
void DNSSD_API resolved(DNSServiceRef ref, DNSServiceFlags flags, std::uint32_t interface,
                        DNSServiceErrorType code, const char* fullName, const char* host,
                        std::uint16_t port, std::uint16_t txtLength, const unsigned char* txt,
                        void* context);
void DNSSD_API addressed(DNSServiceRef ref, DNSServiceFlags flags, std::uint32_t interface,
                         DNSServiceErrorType code, const char* host, const sockaddr* address,
                         std::uint32_t ttl, void* context);

class BonjourDiscovery;

/// A service browsing saw come or go.
struct Browsed {
    bool added;
    std::uint32_t interface;
    const char* name;
    const char* type;
    const char* domain;
};

/// A service browsing found, while it is resolved to its TXT, port and IPv4 address.
struct Found {
    BonjourDiscovery* owner;
    std::string key;
    std::uint32_t interface;
    Sighting sighting;
    Ref resolve;
    Ref address;
};

/// mDNSResponder through dns_sd.h, every call on one thread of its own over one shared connection.
class BonjourDiscovery final : public ServiceDiscovery {
  public:
    explicit BonjourDiscovery(Announcement self) : self_{std::move(self)}, name_{self_.name} {
    }

    ~BonjourDiscovery() override {
        worker_.request_stop();
        wakeUp();
        if (worker_.joinable()) {
            worker_.join();
        }
        for (const int end : wake_) {
            if (end >= 0) {
                ::close(end);
            }
        }
    }

    BonjourDiscovery(const BonjourDiscovery&) = delete;
    BonjourDiscovery& operator=(const BonjourDiscovery&) = delete;
    BonjourDiscovery(BonjourDiscovery&&) = delete;
    BonjourDiscovery& operator=(BonjourDiscovery&&) = delete;

    /// Announces and browses on the worker; false with `error` when it could not.
    bool start(std::string& error) {
        if (::pipe(wake_.data()) != 0) {
            error = std::string{"pipe failed: "} + std::strerror(errno);
            return false;
        }
        for (const int end : wake_) {
            static_cast<void>(::fcntl(end, F_SETFD, FD_CLOEXEC));
            static_cast<void>(::fcntl(end, F_SETFL, O_NONBLOCK));
        }
        std::promise<std::string> started;
        std::future<std::string> answer = started.get_future();
        worker_ = std::jthread{[this, &started](const std::stop_token& stop) {
            run(stop, started);
        }};
        error = answer.get();
        return error.empty();
    }

    std::vector<Sighting> sightings() const override {
        const std::lock_guard lock{mutex_};
        std::vector<Sighting> out;
        for (const auto& [key, sighting] : seen_) {
            if (std::ranges::find(out, sighting.id, &Sighting::id) == out.end()) {
                out.push_back(sighting);
            }
        }
        return out;
    }

    std::string announcedName() const override {
        const std::lock_guard lock{mutex_};
        return name_;
    }

    void announced(DNSServiceErrorType code, const char* name) {
        if (code != kDNSServiceErr_NoError) {
            lucent::warn("lan", "Bonjour could not announce this openSU: {}", errorText(code));
            return;
        }
        std::string was;
        {
            const std::lock_guard lock{mutex_};
            was = std::exchange(name_, name);
        }
        if (was != name) {
            lucent::info("lan", "{} is taken on the LAN; announcing as {}", was, name);
        }
        lucent::info("lan", "announced as {} ({}) on port {}", name, serviceType, self_.port);
    }

    void browse(const Browsed& seen) {
        const std::string key = std::to_string(seen.interface) + "/" + seen.name;
        if (!seen.added) {
            found_.erase(key);
            const std::lock_guard lock{mutex_};
            seen_.erase(key);
            return;
        }
        auto entry = std::make_unique<Found>(Found{.owner = this,
                                                   .key = key,
                                                   .interface = seen.interface,
                                                   .sighting = {},
                                                   .resolve = {},
                                                   .address = {}});
        DNSServiceRef ref = connection_.get();
        if (const DNSServiceErrorType failed =
                DNSServiceResolve(&ref, kDNSServiceFlagsShareConnection, seen.interface, seen.name,
                                  seen.type, seen.domain, resolved, entry.get());
            failed != kDNSServiceErr_NoError) {
            lucent::warn("lan", "Bonjour cannot be asked where {} is: {}", seen.name,
                         errorText(failed));
            return;
        }
        entry->resolve.reset(ref);
        found_.insert_or_assign(key, std::move(entry));
    }

    void resolve(Found& found, DNSServiceErrorType code, const char* host, std::uint16_t port,
                 std::uint16_t txtLength, const unsigned char* txt) {
        if (code != kDNSServiceErr_NoError) {
            lucent::debug("lan", "a service did not resolve: {}", errorText(code));
            return;
        }
        const std::string id = txtValue(txtLength, txt, "id");
        const std::string shown = txtValue(txtLength, txt, "name");
        if (!isPeerId(id) || id == self_.id || txtValue(txtLength, txt, "v") != protocolVersion) {
            found.resolve.reset();
            return;
        }
        const std::string_view instance{found.key.substr(found.key.find('/') + 1)};
        found.sighting = Sighting{.id = id,
                                  .name = shownName(shown.empty() ? instance : shown),
                                  .address = {},
                                  .port = ntohs(port)};
        found.resolve.reset();
        DNSServiceRef ref = connection_.get();
        if (const DNSServiceErrorType failed =
                DNSServiceGetAddrInfo(&ref, kDNSServiceFlagsShareConnection, found.interface,
                                      kDNSServiceProtocol_IPv4, host, addressed, &found);
            failed != kDNSServiceErr_NoError) {
            lucent::warn("lan", "Bonjour cannot be asked for {}'s address: {}", host,
                         errorText(failed));
            return;
        }
        found.address.reset(ref);
    }

    void address(Found& found, DNSServiceFlags flags, DNSServiceErrorType code,
                 const sockaddr* where) {
        if (code != kDNSServiceErr_NoError || (flags & kDNSServiceFlagsAdd) == 0 ||
            where == nullptr || where->sa_family != AF_INET) {
            return;
        }
        std::array<char, INET_ADDRSTRLEN> dotted{};
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(where);
        if (inet_ntop(AF_INET, &ipv4->sin_addr, dotted.data(), dotted.size()) == nullptr) {
            return;
        }
        Sighting sighting = found.sighting;
        sighting.address = dotted.data();
        lucent::info("lan", "found {} at {}:{}", sighting.name, sighting.address, sighting.port);
        const std::lock_guard lock{mutex_};
        seen_.insert_or_assign(found.key, std::move(sighting));
    }

  private:
    void wakeUp() const noexcept {
        const char one = 1;
        if (wake_[1] >= 0) {
            static_cast<void>(::write(wake_[1], &one, sizeof one));
        }
    }

    void run(const std::stop_token& stop, std::promise<std::string>& started) {
        if (std::string failed = connect(); !failed.empty()) {
            found_.clear();
            browser_.reset();
            registration_.reset();
            connection_.reset();
            started.set_value(std::move(failed));
            return;
        }
        started.set_value({});
        while (!stop.stop_requested()) {
            std::array<pollfd, 2> watch{pollfd{.fd = DNSServiceRefSockFD(connection_.get()),
                                               .events = POLLIN,
                                               .revents = 0},
                                        pollfd{.fd = wake_[0], .events = POLLIN, .revents = 0}};
            if (::poll(watch.data(), watch.size(), -1) < 0 && errno != EINTR) {
                lucent::warn("lan", "discovery stopped: poll failed: {}", std::strerror(errno));
                break;
            }
            char drained = 0;
            while (::read(wake_[0], &drained, sizeof drained) > 0) {
            }
            if ((watch[0].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
                continue;
            }
            if (const DNSServiceErrorType failed = DNSServiceProcessResult(connection_.get());
                failed != kDNSServiceErr_NoError) {
                lucent::warn("lan", "discovery stopped: {}", errorText(failed));
                break;
            }
        }
        // mDNSResponder withdraws the announcement when its operation ends; the shared connection
        // goes last.
        found_.clear();
        browser_.reset();
        registration_.reset();
        connection_.reset();
    }

    /// Reaches mDNSResponder, announces this openSU and starts browsing; the reason when it could
    /// not.
    std::string connect() {
        DNSServiceRef ref = nullptr;
        if (const DNSServiceErrorType failed = DNSServiceCreateConnection(&ref);
            failed != kDNSServiceErr_NoError) {
            return "Bonjour cannot be reached: " + errorText(failed);
        }
        connection_.reset(ref);
        TXTRecordRef txt;
        TXTRecordCreate(&txt, 0, nullptr);
        const std::string version{protocolVersion};
        const bool written =
            TXTRecordSetValue(&txt, "id", static_cast<std::uint8_t>(self_.id.size()),
                              self_.id.data()) == kDNSServiceErr_NoError &&
            TXTRecordSetValue(&txt, "name", static_cast<std::uint8_t>(self_.name.size()),
                              self_.name.data()) == kDNSServiceErr_NoError &&
            TXTRecordSetValue(&txt, "v", static_cast<std::uint8_t>(version.size()),
                              version.data()) == kDNSServiceErr_NoError;
        ref = connection_.get();
        const std::string type{serviceType};
        const DNSServiceErrorType announcing =
            written ? DNSServiceRegister(&ref, kDNSServiceFlagsShareConnection, 0,
                                         self_.name.c_str(), type.c_str(), nullptr, nullptr,
                                         htons(self_.port), TXTRecordGetLength(&txt),
                                         TXTRecordGetBytesPtr(&txt), registered, this)
                    : kDNSServiceErr_BadParam;
        TXTRecordDeallocate(&txt);
        if (announcing != kDNSServiceErr_NoError) {
            return "Bonjour cannot announce this openSU: " + errorText(announcing);
        }
        registration_.reset(ref);
        ref = connection_.get();
        if (const DNSServiceErrorType failed = DNSServiceBrowse(
                &ref, kDNSServiceFlagsShareConnection, kDNSServiceInterfaceIndexAny, type.c_str(),
                nullptr, browsed, this);
            failed != kDNSServiceErr_NoError) {
            return "Bonjour cannot look for other openSUs: " + errorText(failed);
        }
        browser_.reset(ref);
        return {};
    }

    Announcement self_;
    std::array<int, 2> wake_{-1, -1};
    // Worker only; the connection first, so the operations sharing it end before it.
    Ref connection_;
    Ref registration_;
    Ref browser_;
    /// Each service browsing found, by its interface and instance name.
    std::map<std::string, std::unique_ptr<Found>> found_;
    // Shared with every thread, under `mutex_`.
    mutable std::mutex mutex_;
    std::string name_;
    /// Each resolved service by its interface and instance name.
    std::map<std::string, Sighting> seen_;
    // Last, so it stops before the members it uses go.
    std::jthread worker_;
};

void DNSSD_API registered(DNSServiceRef /*ref*/, DNSServiceFlags /*flags*/,
                          DNSServiceErrorType code, const char* name, const char* /*type*/,
                          const char* /*domain*/, void* context) {
    static_cast<BonjourDiscovery*>(context)->announced(code, name);
}

void DNSSD_API browsed(DNSServiceRef /*ref*/, DNSServiceFlags flags, std::uint32_t interface,
                       DNSServiceErrorType code, const char* name, const char* type,
                       const char* domain, void* context) {
    if (code != kDNSServiceErr_NoError) {
        lucent::warn("lan", "Bonjour stopped looking for openSUs: {}", errorText(code));
        return;
    }
    static_cast<BonjourDiscovery*>(context)->browse(
        Browsed{.added = (flags & kDNSServiceFlagsAdd) != 0,
                .interface = interface,
                .name = name,
                .type = type,
                .domain = domain});
}

void DNSSD_API resolved(DNSServiceRef /*ref*/, DNSServiceFlags /*flags*/,
                        std::uint32_t /*interface*/, DNSServiceErrorType code,
                        const char* /*fullName*/, const char* host, std::uint16_t port,
                        std::uint16_t txtLength, const unsigned char* txt, void* context) {
    auto* found = static_cast<Found*>(context);
    found->owner->resolve(*found, code, host, port, txtLength, txt);
}

void DNSSD_API addressed(DNSServiceRef /*ref*/, DNSServiceFlags flags, std::uint32_t /*interface*/,
                         DNSServiceErrorType code, const char* /*host*/, const sockaddr* address,
                         std::uint32_t /*ttl*/, void* context) {
    auto* found = static_cast<Found*>(context);
    found->owner->address(*found, flags, code, address);
}

/// `text` as UTF-8; empty when it cannot be.
std::string utf8(CFStringRef text) {
    const CFIndex most =
        CFStringGetMaximumSizeForEncoding(CFStringGetLength(text), kCFStringEncodingUTF8) + 1;
    std::string out(static_cast<std::size_t>(most), '\0');
    if (CFStringGetCString(text, out.data(), most, kCFStringEncodingUTF8) == 0) {
        return {};
    }
    out.resize(std::strlen(out.c_str()));
    return out;
}

} // namespace

std::string machineName() {
    // The name System Settings › General › Sharing shows.
    CFStringRef name = SCDynamicStoreCopyComputerName(nullptr, nullptr);
    if (name == nullptr) {
        return "openSU";
    }
    std::string named = utf8(name);
    CFRelease(name);
    return named.empty() ? std::string{"openSU"} : named;
}

std::unique_ptr<ServiceDiscovery> startDiscovery(const Announcement& self, std::string& error) {
    auto discovery = std::make_unique<BonjourDiscovery>(self);
    if (!discovery->start(error)) {
        return nullptr;
    }
    return discovery;
}

} // namespace opensu::lan
