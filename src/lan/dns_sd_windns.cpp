#include "dns_sd.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include "launch/win32.hpp"

#include <WinDNS.h>

#include "lucent/log.h"
#include "peer_book.hpp"

namespace opensu::lan {
namespace {

using launch::win32::narrow;
using launch::win32::wide;
using Clock = std::chrono::steady_clock;

constexpr std::string_view tooOld = "Finding other openSUs needs Windows 10 version 1809 or newer";
/// Windows' browse never reports a service leaving, so each round asks afresh and a service that
/// answered neither this round nor the last is gone.
constexpr std::chrono::seconds roundLength{10};
/// A service that did not resolve is asked again after this.
constexpr std::chrono::seconds resolveAgain{60};
/// How long starting waits for the announcement, and ending for Windows to let go of its calls.
constexpr std::chrono::seconds settleTime{3};

/// dnsapi.dll's DNS-SD calls, which Windows 10 version 1809 added; looked up when discovery starts
/// so openSU still starts on an older Windows and says why it cannot be found.
class DnsApi {
  public:
    DnsApi() : module_{LoadLibraryExW(L"dnsapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)} {
        if (module_ != nullptr) {
            find(construct, "DnsServiceConstructInstance");
            find(freeInstance, "DnsServiceFreeInstance");
            find(enrol, "DnsServiceRegister");
            find(withdraw, "DnsServiceDeRegister");
            find(browse, "DnsServiceBrowse");
            find(browseCancel, "DnsServiceBrowseCancel");
            find(resolve, "DnsServiceResolve");
        }
    }
    ~DnsApi() {
        if (module_ != nullptr) {
            FreeLibrary(module_);
        }
    }
    DnsApi(const DnsApi&) = delete;
    DnsApi& operator=(const DnsApi&) = delete;
    DnsApi(DnsApi&&) = delete;
    DnsApi& operator=(DnsApi&&) = delete;

    [[nodiscard]] bool ready() const noexcept {
        return construct != nullptr && freeInstance != nullptr && enrol != nullptr &&
               withdraw != nullptr && browse != nullptr && browseCancel != nullptr &&
               resolve != nullptr;
    }

    decltype(&DnsServiceConstructInstance) construct{nullptr};
    decltype(&DnsServiceFreeInstance) freeInstance{nullptr};
    decltype(&DnsServiceRegister) enrol{nullptr};
    decltype(&DnsServiceDeRegister) withdraw{nullptr};
    decltype(&DnsServiceBrowse) browse{nullptr};
    decltype(&DnsServiceBrowseCancel) browseCancel{nullptr};
    decltype(&DnsServiceResolve) resolve{nullptr};

  private:
    template <typename Function> void find(Function& function, const char* name) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        function = reinterpret_cast<Function>(GetProcAddress(module_, name));
    }

    HMODULE module_;
};

/// "a.b.c.d" of an address as Windows keeps it, in network byte order.
std::string dotted(IP4_ADDRESS address) {
    std::array<std::uint8_t, 4> bytes{};
    std::memcpy(bytes.data(), &address, bytes.size());
    return std::to_string(bytes[0]) + "." + std::to_string(bytes[1]) + "." +
           std::to_string(bytes[2]) + "." + std::to_string(bytes[3]);
}

/// This machine's DNS host name, empty when Windows gives none.
std::wstring hostName() {
    DWORD size = 0;
    GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size);
    if (size == 0) {
        return {};
    }
    std::wstring name(size, L'\0');
    if (GetComputerNameExW(ComputerNameDnsHostname, name.data(), &size) == 0) {
        return {};
    }
    name.resize(size);
    return name;
}

/// The instance label `name` is announced under: a dot would split it into DNS labels.
std::string label(std::string name) {
    std::ranges::replace(name, '.', '-');
    return name;
}

/// The label of a full instance name ("Den._opensu._tcp.local").
std::string labelOf(const std::string& instance) {
    return instance.substr(0, instance.find("._"));
}

/// The browse query, and the domain each instance name ends in.
std::wstring serviceDomain() {
    return wide(serviceType) + L".local";
}

/// One call Windows holds, and the context its callbacks get; freed only once Windows said it
/// ended, so a late callback never reaches freed memory.
struct Call {
    struct State* state;
    DNS_SERVICE_CANCEL cancel{};
    std::wstring query;
    bool browse{false};
    /// Whether Windows will call back no more.
    bool ended{false};
};

/// A service a browse round heard of.
struct Service {
    /// The round it last answered in; 0 once it said goodbye.
    std::uint64_t round{0};
    std::optional<Sighting> sighting;
    /// Whether a resolve of it is under way, and when the last one began.
    bool resolving{false};
    std::optional<Clock::time_point> asked;
};

/// Everything Windows' callbacks reach, under `mutex`, on any of its threads.
struct State {
    explicit State(Announcement announced) : self{std::move(announced)}, name{self.name} {
    }

    DnsApi api;
    const Announcement self;
    std::mutex mutex;
    std::condition_variable changed;
    std::string name;
    std::uint64_t round{0};
    /// Each service heard of, by its full instance name.
    std::map<std::wstring, Service> services;
    /// Browses and resolves; a resolve is kept until the end, since Windows may answer one more
    /// than once.
    std::list<Call> calls;
    Call enrolment{.state = this, .cancel = {}, .query = {}, .browse = false, .ended = false};
    DNS_SERVICE_INSTANCE* instance{nullptr};
    std::optional<DWORD> registration;
    bool withdrawn{false};

    [[nodiscard]] bool needsResolving(Clock::time_point now) const {
        return std::ranges::any_of(services, [now](const auto& entry) {
            const Service& service = entry.second;
            return service.round != 0 && !service.sighting && !service.resolving &&
                   (!service.asked || now - *service.asked >= resolveAgain);
        });
    }

    [[nodiscard]] bool settled() const {
        return std::ranges::all_of(calls, &Call::ended) &&
               (registration != DWORD{ERROR_SUCCESS} || withdrawn);
    }
};

void WINAPI registered(DWORD status, void* context, DNS_SERVICE_INSTANCE* instance) {
    State& state = *static_cast<Call*>(context)->state;
    std::string announced;
    if (instance != nullptr) {
        if (instance->pszInstanceName != nullptr) {
            announced = narrow(instance->pszInstanceName);
        }
        if (instance != state.instance) {
            state.api.freeInstance(instance);
        }
    }
    const std::lock_guard lock{state.mutex};
    if (state.registration) {
        state.withdrawn = true;
        state.changed.notify_all();
        return;
    }
    state.registration = status;
    if (status != ERROR_SUCCESS) {
        lucent::warn("lan", "Windows could not announce this openSU: error {}", status);
    } else {
        const std::string taken = labelOf(announced);
        if (!taken.empty() && taken != label(state.self.name)) {
            lucent::info("lan", "{} is taken on the LAN; announcing as {}", state.self.name, taken);
            state.name = taken;
        }
        lucent::info("lan", "announced as {} ({}) on port {}", state.name, serviceType,
                     state.self.port);
    }
    state.changed.notify_all();
}

void WINAPI browsed(DWORD status, void* context, DNS_RECORD* records) {
    auto& call = *static_cast<Call*>(context);
    State& state = *call.state;
    const std::lock_guard lock{state.mutex};
    if (status != ERROR_SUCCESS) {
        call.ended = true;
        if (status != ERROR_CANCELLED) {
            lucent::warn("lan", "Windows stopped looking for openSUs: error {}", status);
        }
    }
    // DnsServiceBrowse answers in wide records whether or not UNICODE is defined.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    for (auto* record = reinterpret_cast<DNS_RECORDW*>(records); record != nullptr;
         record = record->pNext) {
        if (record->wType != DNS_TYPE_PTR || record->Data.PTR.pNameHost == nullptr) {
            continue;
        }
        Service& service = state.services[record->Data.PTR.pNameHost];
        if (record->dwTtl == 0) {
            service.round = 0;
            service.sighting.reset();
        } else {
            service.round = state.round;
        }
    }
    if (records != nullptr) {
        DnsFree(records, DnsFreeRecordList);
    }
    state.changed.notify_all();
}

/// What a resolved instance says: another openSU of this protocol at an IPv4 address.
std::optional<Sighting> sightingOf(const DNS_SERVICE_INSTANCE& instance, const std::string& self) {
    std::map<std::string, std::string, std::less<>> txt;
    for (DWORD index = 0; index < instance.dwPropertyCount; ++index) {
        if (instance.keys[index] != nullptr && instance.values[index] != nullptr) {
            txt.emplace(narrow(instance.keys[index]), narrow(instance.values[index]));
        }
    }
    const auto id = txt.find("id");
    const auto shown = txt.find("name");
    const auto version = txt.find("v");
    if (id == txt.end() || !isPeerId(id->second) || id->second == self || version == txt.end() ||
        version->second != protocolVersion || instance.ip4Address == nullptr) {
        return std::nullopt;
    }
    const std::string name = shown != txt.end() ? shown->second
                             : instance.pszInstanceName != nullptr
                                 ? labelOf(narrow(instance.pszInstanceName))
                                 : std::string{};
    return Sighting{.id = id->second,
                    .name = shownName(name),
                    .address = dotted(*instance.ip4Address),
                    .port = instance.wPort};
}

void WINAPI resolved(DWORD status, void* context, DNS_SERVICE_INSTANCE* instance) {
    auto& call = *static_cast<Call*>(context);
    State& state = *call.state;
    std::optional<Sighting> sighting;
    if (instance != nullptr) {
        if (status == ERROR_SUCCESS) {
            sighting = sightingOf(*instance, state.self.id);
        }
        state.api.freeInstance(instance);
    }
    const std::lock_guard lock{state.mutex};
    call.ended = true;
    const auto service = state.services.find(call.query);
    if (service == state.services.end()) {
        return;
    }
    service->second.resolving = false;
    if (sighting) {
        if (service->second.sighting != sighting) {
            lucent::info("lan", "found {} at {}:{}", sighting->name, sighting->address,
                         sighting->port);
        }
        service->second.sighting = std::move(sighting);
    } else if (status != ERROR_SUCCESS) {
        lucent::debug("lan", "a service did not resolve: error {}", status);
    }
}

/// Windows' DNS-SD (dnsapi.dll): the announcement, and browse rounds whose services are resolved
/// by a worker of its own; every answer arrives on one of Windows' threads.
class WindowsDiscovery final : public ServiceDiscovery {
  public:
    explicit WindowsDiscovery(std::unique_ptr<State> state) : state_{std::move(state)} {
    }

    ~WindowsDiscovery() override {
        worker_.request_stop();
        {
            const std::lock_guard lock{state_->mutex};
            state_->changed.notify_all();
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        State& state = *state_;
        cancelBrowsing();
        bool announced = false;
        {
            const std::lock_guard lock{state.mutex};
            announced = state.registration == DWORD{ERROR_SUCCESS};
        }
        if (announced && state.api.withdraw(&request_, nullptr) != DNS_REQUEST_PENDING) {
            const std::lock_guard lock{state.mutex};
            state.withdrawn = true;
        }
        bool settled = false;
        {
            std::unique_lock lock{state.mutex};
            settled = state.changed.wait_for(lock, settleTime, [&state] {
                return state.settled();
            });
        }
        if (!settled) {
            // Windows may still call into it, so it is left to the process rather than freed.
            [[maybe_unused]] const State* kept = state_.release();
            lucent::warn("lan", "Windows still holds openSU's DNS-SD calls; their state is kept");
            return;
        }
        if (state.instance != nullptr) {
            state.api.freeInstance(state.instance);
        }
    }

    WindowsDiscovery(const WindowsDiscovery&) = delete;
    WindowsDiscovery& operator=(const WindowsDiscovery&) = delete;
    WindowsDiscovery(WindowsDiscovery&&) = delete;
    WindowsDiscovery& operator=(WindowsDiscovery&&) = delete;

    /// Announces and starts browsing; false with `error` when it could not.
    bool start(std::string& error) {
        if (!announce(error) || !browse(error)) {
            return false;
        }
        worker_ = std::jthread{[this](const std::stop_token& stop) {
            run(stop);
        }};
        return true;
    }

    std::vector<Sighting> sightings() const override {
        const std::lock_guard lock{state_->mutex};
        std::vector<Sighting> out;
        for (const auto& [name, service] : state_->services) {
            if (service.sighting &&
                std::ranges::find(out, service.sighting->id, &Sighting::id) == out.end()) {
                out.push_back(*service.sighting);
            }
        }
        return out;
    }

    std::string announcedName() const override {
        const std::lock_guard lock{state_->mutex};
        return state_->name;
    }

  private:
    bool announce(std::string& error) {
        State& state = *state_;
        const std::wstring host = hostName();
        if (host.empty()) {
            error = "Windows gives this machine no host name: error " + launch::win32::lastError();
            return false;
        }
        const std::array<std::wstring, 3> keys{L"id", L"name", L"v"};
        const std::array<std::wstring, 3> values{wide(state.self.id), wide(state.self.name),
                                                 wide(protocolVersion)};
        std::array<PCWSTR, 3> keyText{keys[0].c_str(), keys[1].c_str(), keys[2].c_str()};
        std::array<PCWSTR, 3> valueText{values[0].c_str(), values[1].c_str(), values[2].c_str()};
        const std::wstring instance = wide(label(state.self.name)) + L"." + serviceDomain();
        const std::wstring target = host + L".local";
        state.instance = state.api.construct(instance.c_str(), target.c_str(), nullptr, nullptr,
                                             state.self.port, 0, 0, static_cast<DWORD>(keys.size()),
                                             keyText.data(), valueText.data());
        if (state.instance == nullptr) {
            error = "Windows cannot describe this openSU's service";
            return false;
        }
        request_ = DNS_SERVICE_REGISTER_REQUEST{.Version = DNS_QUERY_REQUEST_VERSION1,
                                                .InterfaceIndex = 0,
                                                .pServiceInstance = state.instance,
                                                .pRegisterCompletionCallback = registered,
                                                .pQueryContext = &state.enrolment,
                                                .hCredentials = nullptr,
                                                .unicastEnabled = FALSE};
        if (const DWORD started = state.api.enrol(&request_, nullptr);
            started != DNS_REQUEST_PENDING) {
            error = "Windows cannot announce this openSU: error " + std::to_string(started);
            return false;
        }
        std::unique_lock lock{state.mutex};
        state.changed.wait_for(lock, settleTime, [&state] {
            return state.registration.has_value();
        });
        if (state.registration && *state.registration != ERROR_SUCCESS) {
            error = "Windows could not announce this openSU: error " +
                    std::to_string(*state.registration);
            return false;
        }
        return true;
    }

    /// Starts the next browse round; false with `error` when Windows refused it.
    bool browse(std::string& error) {
        State& state = *state_;
        Call* call = nullptr;
        {
            const std::lock_guard lock{state.mutex};
            ++state.round;
            std::erase_if(state.calls, [](const Call& held) {
                return held.browse && held.ended;
            });
            call = &state.calls.emplace_back(Call{.state = &state,
                                                  .cancel = {},
                                                  .query = serviceDomain(),
                                                  .browse = true,
                                                  .ended = false});
        }
        DNS_SERVICE_BROWSE_REQUEST request{};
        request.Version = DNS_QUERY_REQUEST_VERSION1;
        request.InterfaceIndex = 0;
        request.QueryName = call->query.c_str();
        request.pBrowseCallback = browsed;
        request.pQueryContext = call;
        const DNS_STATUS started = state.api.browse(&request, &call->cancel);
        const std::lock_guard lock{state.mutex};
        if (started != DNS_REQUEST_PENDING) {
            call->ended = true;
            error = "Windows cannot look for other openSUs: error " + std::to_string(started);
            return false;
        }
        browsing_ = call;
        return true;
    }

    /// Cancels the browse round under way, outside the lock its callback takes.
    void cancelBrowsing() {
        Call* call = nullptr;
        {
            const std::lock_guard lock{state_->mutex};
            call = std::exchange(browsing_, nullptr);
            if (call != nullptr && call->ended) {
                call = nullptr;
            }
        }
        if (call != nullptr) {
            state_->api.browseCancel(&call->cancel);
        }
    }

    void run(const std::stop_token& stop) {
        State& state = *state_;
        Clock::time_point roundEnds = Clock::now() + roundLength;
        while (!stop.stop_requested()) {
            resolveHeard();
            {
                std::unique_lock lock{state.mutex};
                state.changed.wait_until(lock, roundEnds, [&state, &stop] {
                    return stop.stop_requested() || state.needsResolving(Clock::now());
                });
            }
            if (stop.stop_requested() || Clock::now() < roundEnds) {
                continue;
            }
            cancelBrowsing();
            {
                const std::lock_guard lock{state.mutex};
                const std::uint64_t kept = state.round - 1;
                std::erase_if(state.services, [kept](const auto& entry) {
                    return entry.second.round < kept && !entry.second.resolving;
                });
            }
            if (std::string error; !browse(error)) {
                lucent::warn("lan", "{}", error);
            }
            roundEnds = Clock::now() + roundLength;
        }
    }

    /// Resolves each service heard of that has no sighting and is not being resolved.
    void resolveHeard() {
        State& state = *state_;
        std::vector<Call*> asking;
        {
            const std::lock_guard lock{state.mutex};
            const Clock::time_point now = Clock::now();
            for (auto& [name, service] : state.services) {
                if (service.round == 0 || service.sighting || service.resolving ||
                    (service.asked && now - *service.asked < resolveAgain)) {
                    continue;
                }
                service.resolving = true;
                service.asked = now;
                asking.push_back(&state.calls.emplace_back(Call{.state = &state,
                                                                .cancel = {},
                                                                .query = name,
                                                                .browse = false,
                                                                .ended = false}));
            }
        }
        for (Call* call : asking) {
            DNS_SERVICE_RESOLVE_REQUEST request{};
            request.Version = DNS_QUERY_REQUEST_VERSION1;
            request.InterfaceIndex = 0;
            request.QueryName = call->query.data();
            request.pResolveCompletionCallback = resolved;
            request.pQueryContext = call;
            const DNS_STATUS started = state.api.resolve(&request, &call->cancel);
            if (started == DNS_REQUEST_PENDING) {
                continue;
            }
            const std::lock_guard lock{state.mutex};
            call->ended = true;
            if (const auto service = state.services.find(call->query);
                service != state.services.end()) {
                service->second.resolving = false;
            }
            lucent::warn("lan", "Windows cannot be asked where {} is: error {}",
                         narrow(call->query), started);
        }
    }

    std::unique_ptr<State> state_;
    DNS_SERVICE_REGISTER_REQUEST request_{};
    /// The browse round under way, under the state's mutex.
    Call* browsing_{nullptr};
    // Last, so it stops before the members it uses go.
    std::jthread worker_;
};

} // namespace

std::string machineName() {
    const std::wstring name = hostName();
    return name.empty() ? std::string{"openSU"} : narrow(name);
}

std::unique_ptr<ServiceDiscovery> startDiscovery(const Announcement& self, std::string& error) {
    auto state = std::make_unique<State>(self);
    if (!state->api.ready()) {
        error = std::string{tooOld};
        return nullptr;
    }
    auto discovery = std::make_unique<WindowsDiscovery>(std::move(state));
    if (!discovery->start(error)) {
        return nullptr;
    }
    return discovery;
}

} // namespace opensu::lan
