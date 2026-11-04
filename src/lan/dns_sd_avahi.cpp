#include "dns_sd.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>

#include <poll.h>
#include <sys/eventfd.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "host/sd_bus.hpp"
#include "lucent/log.h"
#include "peer_book.hpp"

namespace opensu::lan {
namespace {

using host::sdbus::Bus;
using host::sdbus::BusError;
using host::sdbus::Message;
using host::sdbus::Slot;

constexpr const char* avahiName = "org.freedesktop.Avahi";
constexpr const char* serverPath = "/";
constexpr const char* serverInterface = "org.freedesktop.Avahi.Server";
constexpr const char* groupInterface = "org.freedesktop.Avahi.EntryGroup";
constexpr const char* browserInterface = "org.freedesktop.Avahi.ServiceBrowser";
// avahi-common/address.h and defs.h.
constexpr std::int32_t anyInterface = -1;
constexpr std::int32_t ipv4 = 0;
constexpr std::int32_t groupCollision = 3;
constexpr std::int32_t groupFailure = 4;
constexpr std::size_t hostNameBytes = 256;

/// The TXT entries a service carries.
std::map<std::string, std::string, std::less<>> readTxt(sd_bus_message* message) {
    std::map<std::string, std::string, std::less<>> entries;
    if (sd_bus_message_enter_container(message, SD_BUS_TYPE_ARRAY, "ay") <= 0) {
        return entries;
    }
    while (sd_bus_message_at_end(message, 0) == 0) {
        const void* data = nullptr;
        std::size_t size = 0;
        if (sd_bus_message_read_array(message, 'y', &data, &size) < 0) {
            break;
        }
        const std::string_view entry{static_cast<const char*>(data), size};
        const std::size_t equals = entry.find('=');
        if (equals != std::string_view::npos) {
            entries.emplace(std::string{entry.substr(0, equals)},
                            std::string{entry.substr(equals + 1)});
        }
    }
    sd_bus_message_exit_container(message);
    return entries;
}

int browseSignal(sd_bus_message* message, void* discovery, sd_bus_error* error);
int groupSignal(sd_bus_message* message, void* discovery, sd_bus_error* error);
int resolved(sd_bus_message* message, void* discovery, sd_bus_error* error);

class AvahiDiscovery final : public ServiceDiscovery {
  public:
    explicit AvahiDiscovery(Announcement self)
        : self_{std::move(self)}, wake_{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)}, name_{self_.name} {
    }

    ~AvahiDiscovery() override {
        worker_.request_stop();
        wakeUp();
        if (worker_.joinable()) {
            worker_.join();
        }
        if (wake_ >= 0) {
            ::close(wake_);
        }
    }

    AvahiDiscovery(const AvahiDiscovery&) = delete;
    AvahiDiscovery& operator=(const AvahiDiscovery&) = delete;
    AvahiDiscovery(AvahiDiscovery&&) = delete;
    AvahiDiscovery& operator=(AvahiDiscovery&&) = delete;

    /// Announces and browses on the worker; false with `error` when it could not.
    bool start(std::string& error) {
        if (wake_ < 0) {
            error = std::string{"eventfd failed: "} + std::strerror(errno);
            return false;
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

    int browsed(sd_bus_message* message) {
        if (!fromAvahi(message)) {
            return 0;
        }
        const std::string_view member = sd_bus_message_get_member(message);
        if (member == "Failure") {
            const char* why = nullptr;
            static_cast<void>(sd_bus_message_read(message, "s", &why));
            lucent::warn("lan", "Avahi stopped looking for openSUs: {}", why != nullptr ? why : "");
            return 0;
        }
        if (member != "ItemNew" && member != "ItemRemove") {
            return 0;
        }
        std::int32_t interface = 0;
        std::int32_t protocol = 0;
        const char* name = nullptr;
        const char* type = nullptr;
        const char* domain = nullptr;
        std::uint32_t flags = 0;
        if (sd_bus_message_read(message, "iisssu", &interface, &protocol, &name, &type, &domain,
                                &flags) < 0) {
            return 0;
        }
        if (member == "ItemRemove") {
            const std::lock_guard lock{mutex_};
            seen_.erase(keyOf(interface, name));
            return 0;
        }
        const int code = sd_bus_call_method_async(
            bus_.get(), nullptr, avahiName, serverPath, serverInterface, "ResolveService", resolved,
            this, "iisssiu", interface, protocol, name, type, domain, ipv4, std::uint32_t{0});
        if (code < 0) {
            lucent::warn("lan", "Avahi cannot be asked where {} is: {}", name,
                         std::strerror(-code));
        }
        return 0;
    }

    int resolve(sd_bus_message* reply) {
        if (sd_bus_message_is_method_error(reply, nullptr) != 0) {
            const sd_bus_error* why = sd_bus_message_get_error(reply);
            lucent::debug("lan", "a service did not resolve: {}",
                          why != nullptr && why->message != nullptr ? why->message : "");
            return 0;
        }
        std::int32_t interface = 0;
        std::int32_t protocol = 0;
        const char* name = nullptr;
        const char* type = nullptr;
        const char* domain = nullptr;
        const char* host = nullptr;
        std::int32_t addressProtocol = 0;
        const char* address = nullptr;
        std::uint16_t port = 0;
        if (sd_bus_message_read(reply, "iissssisq", &interface, &protocol, &name, &type, &domain,
                                &host, &addressProtocol, &address, &port) < 0) {
            return 0;
        }
        const auto txt = readTxt(reply);
        const auto id = txt.find("id");
        const auto shown = txt.find("name");
        const auto version = txt.find("v");
        if (id == txt.end() || !isPeerId(id->second) || id->second == self_.id ||
            version == txt.end() || version->second != protocolVersion || addressProtocol != ipv4) {
            return 0;
        }
        Sighting sighting{.id = id->second,
                          .name = shownName(shown != txt.end() ? shown->second : name),
                          .address = address,
                          .port = port};
        lucent::info("lan", "found {} at {}:{}", sighting.name, sighting.address, sighting.port);
        const std::lock_guard lock{mutex_};
        seen_.insert_or_assign(keyOf(interface, name), std::move(sighting));
        return 0;
    }

    int groupChanged(sd_bus_message* message) {
        if (!fromAvahi(message)) {
            return 0;
        }
        std::int32_t state = 0;
        const char* why = nullptr;
        if (sd_bus_message_read(message, "is", &state, &why) < 0) {
            return 0;
        }
        if (state == groupCollision) {
            renameAfterCollision();
        } else if (state == groupFailure) {
            lucent::warn("lan", "Avahi could not announce this openSU: {}",
                         why != nullptr ? why : "");
        }
        return 0;
    }

  private:
    static std::string keyOf(std::int32_t interface, const char* name) {
        return std::to_string(interface) + "/" + name;
    }

    void wakeUp() const noexcept {
        const std::uint64_t one = 1;
        static_cast<void>(::write(wake_, &one, sizeof one));
    }

    [[nodiscard]] bool fromAvahi(sd_bus_message* message) const {
        const char* sender = sd_bus_message_get_sender(message);
        return sender != nullptr && avahi_ == sender;
    }

    void run(const std::stop_token& stop, std::promise<std::string>& started) {
        if (std::string failed = connect(); !failed.empty()) {
            started.set_value(std::move(failed));
            return;
        }
        started.set_value({});
        while (!stop.stop_requested()) {
            std::array<pollfd, 2> watch{
                pollfd{.fd = sd_bus_get_fd(bus_.get()),
                       .events = static_cast<short>(sd_bus_get_events(bus_.get())),
                       .revents = 0},
                pollfd{.fd = wake_, .events = POLLIN, .revents = 0}};
            if (poll(watch.data(), watch.size(), host::sdbus::pollTimeout(bus_.get())) < 0 &&
                errno != EINTR) {
                lucent::warn("lan", "discovery stopped: poll failed: {}", std::strerror(errno));
                break;
            }
            std::uint64_t woken = 0;
            static_cast<void>(::read(wake_, &woken, sizeof woken));
            int processed = 0;
            while ((processed = sd_bus_process(bus_.get(), nullptr)) > 0) {
            }
            if (processed < 0) {
                lucent::warn("lan", "discovery stopped: the system bus failed: {}",
                             std::strerror(-processed));
                break;
            }
        }
        // Avahi drops a client's announcement and browser when its connection closes.
        bus_.reset();
    }

    /// Reaches Avahi, announces this openSU and starts browsing; the reason when it could not.
    std::string connect() {
        sd_bus* opened = nullptr;
        if (const int code = sd_bus_open_system(&opened); code < 0) {
            return std::string{"the system bus cannot be reached: "} + std::strerror(-code);
        }
        bus_.reset(opened);
        if (!findAvahi()) {
            return "Avahi is not running; start avahi-daemon to find other openSUs";
        }
        sd_bus_slot* slot = nullptr;
        if (const int code = sd_bus_match_signal(bus_.get(), &slot, nullptr, nullptr,
                                                 browserInterface, nullptr, browseSignal, this);
            code < 0) {
            return std::string{"Avahi's answers cannot be listened for: "} + std::strerror(-code);
        }
        browseSlot_.reset(slot);
        if (const int code = sd_bus_match_signal(bus_.get(), &slot, nullptr, nullptr,
                                                 groupInterface, "StateChanged", groupSignal, this);
            code < 0) {
            return std::string{"Avahi's answers cannot be listened for: "} + std::strerror(-code);
        }
        groupSlot_.reset(slot);
        BusError error;
        sd_bus_message* reply = nullptr;
        int code = sd_bus_call_method(bus_.get(), avahiName, serverPath, serverInterface,
                                      "EntryGroupNew", error.get(), &reply, "");
        Message owned{reply};
        const char* path = nullptr;
        if (code < 0 || sd_bus_message_read(reply, "o", &path) < 0) {
            return "Avahi cannot announce this openSU: " + error.text(code);
        }
        group_ = path;
        if (std::string failed = announce(); !failed.empty()) {
            return failed;
        }
        BusError browseError;
        code = sd_bus_call_method(bus_.get(), avahiName, serverPath, serverInterface,
                                  "ServiceBrowserNew", browseError.get(), nullptr, "iissu",
                                  anyInterface, ipv4, std::string{serviceType}.c_str(), "",
                                  std::uint32_t{0});
        if (code < 0) {
            return "Avahi cannot look for other openSUs: " + browseError.text(code);
        }
        lucent::info("lan", "announced as {} ({}) on port {}", name_, serviceType, self_.port);
        return {};
    }

    /// Avahi's unique bus name, the only sender listened to.
    bool findAvahi() {
        BusError error;
        sd_bus_message* reply = nullptr;
        const int code = sd_bus_call_method(bus_.get(), "org.freedesktop.DBus",
                                            "/org/freedesktop/DBus", "org.freedesktop.DBus",
                                            "GetNameOwner", error.get(), &reply, "s", avahiName);
        const Message owned{reply};
        const char* owner = nullptr;
        if (code < 0 || sd_bus_message_read(reply, "s", &owner) < 0) {
            lucent::warn("lan", "Avahi is not on the system bus: {}", error.text(code));
            return false;
        }
        avahi_ = owner;
        return true;
    }

    /// Adds the service under the name now chosen and commits it.
    std::string announce() {
        sd_bus_message* raw = nullptr;
        int code = sd_bus_message_new_method_call(bus_.get(), &raw, avahiName, group_.c_str(),
                                                  groupInterface, "AddService");
        const Message call{raw};
        if (code < 0) {
            return std::string{"Avahi cannot announce this openSU: "} + std::strerror(-code);
        }
        const std::string name = announcedName();
        const std::array<std::string, 3> txt{"id=" + self_.id, "name=" + self_.name,
                                             "v=" + std::string{protocolVersion}};
        code = sd_bus_message_append(call.get(), "iiussssq", anyInterface, ipv4, std::uint32_t{0},
                                     name.c_str(), std::string{serviceType}.c_str(), "", "",
                                     self_.port);
        if (code >= 0) {
            code = sd_bus_message_open_container(call.get(), SD_BUS_TYPE_ARRAY, "ay");
        }
        for (const std::string& entry : txt) {
            if (code >= 0) {
                code = sd_bus_message_append_array(call.get(), 'y', entry.data(), entry.size());
            }
        }
        if (code >= 0) {
            code = sd_bus_message_close_container(call.get());
        }
        BusError error;
        if (code >= 0) {
            code = sd_bus_call(bus_.get(), call.get(), 0, error.get(), nullptr);
        }
        if (code < 0) {
            return "Avahi cannot announce this openSU: " + error.text(code);
        }
        BusError commitError;
        code = sd_bus_call_method(bus_.get(), avahiName, group_.c_str(), groupInterface, "Commit",
                                  commitError.get(), nullptr, "");
        if (code < 0) {
            return "Avahi cannot announce this openSU: " + commitError.text(code);
        }
        return {};
    }

    /// Another service on the LAN has the name: Avahi's next one ("name #2") is announced.
    void renameAfterCollision() {
        BusError error;
        sd_bus_message* reply = nullptr;
        const std::string was = announcedName();
        int code =
            sd_bus_call_method(bus_.get(), avahiName, serverPath, serverInterface,
                               "GetAlternativeServiceName", error.get(), &reply, "s", was.c_str());
        const Message owned{reply};
        const char* next = nullptr;
        if (code < 0 || sd_bus_message_read(reply, "s", &next) < 0) {
            lucent::warn("lan", "{} is taken on the LAN and Avahi has no other name: {}", was,
                         error.text(code));
            return;
        }
        {
            const std::lock_guard lock{mutex_};
            name_ = next;
        }
        BusError resetError;
        code = sd_bus_call_method(bus_.get(), avahiName, group_.c_str(), groupInterface, "Reset",
                                  resetError.get(), nullptr, "");
        if (code < 0) {
            lucent::warn("lan", "Avahi cannot rename this openSU: {}", resetError.text(code));
            return;
        }
        lucent::info("lan", "{} is taken on the LAN; announcing as {}", was, next);
        if (const std::string failed = announce(); !failed.empty()) {
            lucent::warn("lan", "{}", failed);
        }
    }

    Announcement self_;
    int wake_;
    // Worker only.
    Bus bus_;
    Slot browseSlot_;
    Slot groupSlot_;
    std::string avahi_;
    std::string group_;
    // Shared with every thread, under `mutex_`.
    mutable std::mutex mutex_;
    std::string name_;
    /// Each resolved service by its interface and instance name.
    std::map<std::string, Sighting> seen_;
    // Last, so it stops before the members it uses go.
    std::jthread worker_;
};

int browseSignal(sd_bus_message* message, void* discovery, sd_bus_error* /*error*/) {
    return static_cast<AvahiDiscovery*>(discovery)->browsed(message);
}

int groupSignal(sd_bus_message* message, void* discovery, sd_bus_error* /*error*/) {
    return static_cast<AvahiDiscovery*>(discovery)->groupChanged(message);
}

int resolved(sd_bus_message* message, void* discovery, sd_bus_error* /*error*/) {
    return static_cast<AvahiDiscovery*>(discovery)->resolve(message);
}

} // namespace

std::string machineName() {
    std::array<char, hostNameBytes> name{};
    if (gethostname(name.data(), name.size() - 1) != 0 || name.front() == '\0') {
        return "openSU";
    }
    return name.data();
}

std::string_view discoveryMissing() noexcept {
    return {};
}

std::unique_ptr<ServiceDiscovery> startDiscovery(const Announcement& self, std::string& error) {
    auto discovery = std::make_unique<AvahiDiscovery>(self);
    if (!discovery->start(error)) {
        return nullptr;
    }
    return discovery;
}

} // namespace opensu::lan
