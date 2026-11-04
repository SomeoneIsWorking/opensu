#include "polkit_agent.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <future>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include <poll.h>
#include <pwd.h>
#include <sys/eventfd.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "launch/procfs.hpp"
#include "lucent/log.h"
#include "polkit_helper.hpp"
#include "sd_bus.hpp"
#include "secrets/secret.hpp"

namespace opensu::host {
namespace {

namespace fs = std::filesystem;

constexpr const char* authorityName = "org.freedesktop.PolicyKit1";
constexpr const char* authorityPath = "/org/freedesktop/PolicyKit1/Authority";
constexpr const char* authorityInterface = "org.freedesktop.PolicyKit1.Authority";
constexpr const char* agentInterface = "org.freedesktop.PolicyKit1.AuthenticationAgent";
constexpr const char* agentPath = "/org/opensu/PolkitAgent";
constexpr const char* cancelledError = "org.freedesktop.PolicyKit1.Error.Cancelled";
constexpr const char* failedError = "org.freedesktop.PolicyKit1.Error.Failed";
/// A process subject's fields: its pid, start time and owner, as polkit keys them.
constexpr int subjectFields = 3;

using sdbus::Bus;
using sdbus::BusError;
using sdbus::Message;
using sdbus::Slot;

std::optional<fs::path> installedHelper() {
    for (const std::string_view path : polkit::helperPaths) {
        std::error_code ec;
        if (fs::is_regular_file(path, ec)) {
            return fs::path{path};
        }
    }
    return std::nullopt;
}

std::string userName(uid_t uid) {
    constexpr std::size_t entryBytes = 16384;
    std::vector<char> buffer(entryBytes);
    passwd entry{};
    passwd* found = nullptr;
    if (getpwuid_r(uid, &entry, buffer.data(), buffer.size(), &found) != 0 || found == nullptr) {
        return {};
    }
    return found->pw_name;
}

/// The uids of the unix-user identities in polkit's `a(sa{sv})`, read from `message`.
std::vector<uid_t> identityUids(sd_bus_message* message) {
    std::vector<uid_t> uids;
    if (sd_bus_message_enter_container(message, SD_BUS_TYPE_ARRAY, "(sa{sv})") <= 0) {
        return uids;
    }
    while (sd_bus_message_enter_container(message, SD_BUS_TYPE_STRUCT, "sa{sv}") > 0) {
        const char* kind = nullptr;
        if (sd_bus_message_read(message, "s", &kind) < 0 ||
            sd_bus_message_enter_container(message, SD_BUS_TYPE_ARRAY, "{sv}") <= 0) {
            return uids;
        }
        while (sd_bus_message_enter_container(message, SD_BUS_TYPE_DICT_ENTRY, "sv") > 0) {
            const char* key = nullptr;
            std::uint32_t uid = 0;
            if (sd_bus_message_read(message, "s", &key) < 0) {
                return uids;
            }
            if (std::string_view{kind} == "unix-user" && std::string_view{key} == "uid" &&
                sd_bus_message_read(message, "v", "u", &uid) >= 0) {
                uids.push_back(uid);
            } else if (sd_bus_message_skip(message, "v") < 0) {
                return uids;
            }
            sd_bus_message_exit_container(message);
        }
        sd_bus_message_exit_container(message);
        sd_bus_message_exit_container(message);
    }
    sd_bus_message_exit_container(message);
    return uids;
}

int beginCall(sd_bus_message* message, void* agent, sd_bus_error* error);
int cancelCall(sd_bus_message* message, void* agent, sd_bus_error* error);

const sd_bus_vtable agentVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("BeginAuthentication", "sssa{ss}sa(sa{sv})", "", beginCall,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("CancelAuthentication", "s", "", cancelCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

class SdBusPolkitAgent final : public PolkitAgent {
  public:
    SdBusPolkitAgent(fs::path helper, std::string locale, std::uint64_t startTime)
        : helper_{std::move(helper)}, locale_{std::move(locale)}, startTime_{startTime},
          wake_{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)} {
    }

    ~SdBusPolkitAgent() override {
        worker_.request_stop();
        wakeUp();
        if (worker_.joinable()) {
            worker_.join();
        }
        if (wake_ >= 0) {
            ::close(wake_);
        }
    }

    SdBusPolkitAgent(const SdBusPolkitAgent&) = delete;
    SdBusPolkitAgent& operator=(const SdBusPolkitAgent&) = delete;
    SdBusPolkitAgent(SdBusPolkitAgent&&) = delete;
    SdBusPolkitAgent& operator=(SdBusPolkitAgent&&) = delete;

    /// Registers on the worker thread; false, with the reason logged, when it could not.
    bool start() {
        if (wake_ < 0) {
            lucent::warn("polkit", "no agent: eventfd failed: {}", std::strerror(errno));
            return false;
        }
        std::promise<bool> registered;
        std::future<bool> answer = registered.get_future();
        worker_ = std::jthread{[this, &registered](const std::stop_token& stop) {
            run(stop, registered);
        }};
        return answer.get();
    }

    std::optional<polkit::Prompt> prompt() const override {
        const std::lock_guard lock{mutex_};
        return shown_;
    }

    void answer(std::uint64_t serial, std::string& secret) override {
        {
            const std::lock_guard lock{mutex_};
            secrets::wipe(answer_);
            answer_.swap(secret);
            answerSerial_ = serial;
            hasAnswer_ = true;
            shown_ = withInbox(std::move(shown_));
        }
        wakeUp();
    }

    void cancel() override {
        {
            const std::lock_guard lock{mutex_};
            cancelWanted_ = true;
            shown_ = withInbox(std::move(shown_));
        }
        wakeUp();
    }

    void shutDown() override {
        {
            const std::lock_guard lock{mutex_};
            closing_ = true;
            shown_ = withInbox(std::move(shown_));
        }
        wakeUp();
    }

    /// polkit's BeginAuthentication: kept unanswered until the authentication has an outcome.
    int begin(sd_bus_message* message, sd_bus_error* error) {
        if (!fromAuthority(message)) {
            return sd_bus_error_set(error, SD_BUS_ERROR_ACCESS_DENIED, "only polkit asks");
        }
        const char* action = nullptr;
        const char* text = nullptr;
        const char* icon = nullptr;
        const char* cookie = nullptr;
        if (sd_bus_message_read(message, "sss", &action, &text, &icon) < 0 ||
            sd_bus_message_skip(message, "a{ss}") < 0 ||
            sd_bus_message_read(message, "s", &cookie) < 0) {
            return sd_bus_error_set(error, SD_BUS_ERROR_INVALID_ARGS, "unreadable request");
        }
        const std::vector<uid_t> uids = identityUids(message);
        if (uids.empty()) {
            return sd_bus_error_set(error, failedError, "no user to authenticate");
        }
        if (pending_) {
            return sd_bus_error_set(error, failedError, "another authentication is under way");
        }
        if (closingNow()) {
            lucent::info("polkit", "{} asked while openSU is closing; cancelled", action);
            return sd_bus_error_set(error, cancelledError, "openSU is closing");
        }
        // The player's own password when polkit takes it, else the first administrator's.
        const auto own = std::ranges::find(uids, getuid());
        const std::string user = userName(own != uids.end() ? *own : uids.front());
        if (user.empty()) {
            return sd_bus_error_set(error, failedError, "the user has no name");
        }
        lucent::info("polkit", "{} asks for {}'s password", action, user);
        pending_.emplace(Pending{cookie, Message{sd_bus_message_ref(message)},
                                 polkit::Authentication{helper_, text, user, cookie, serials_}});
        pending_->authentication.begin();
        settle();
        publish();
        return 1;
    }

    /// polkit's CancelAuthentication.
    int cancelled(sd_bus_message* message, sd_bus_error* error) {
        if (!fromAuthority(message)) {
            return sd_bus_error_set(error, SD_BUS_ERROR_ACCESS_DENIED, "only polkit asks");
        }
        const char* cookie = nullptr;
        if (sd_bus_message_read(message, "s", &cookie) < 0) {
            return sd_bus_error_set(error, SD_BUS_ERROR_INVALID_ARGS, "unreadable request");
        }
        if (pending_ && pending_->cookie == cookie) {
            lucent::info("polkit", "polkit cancelled the authentication");
            reply(cancelledError, "polkit cancelled it");
            publish();
        }
        return sd_bus_reply_method_return(message, "");
    }

  private:
    /// One authentication in hand: polkit's call, waiting for its reply.
    struct Pending {
        std::string cookie;
        Message call;
        polkit::Authentication authentication;
    };

    [[nodiscard]] bool closingNow() const {
        const std::lock_guard lock{mutex_};
        return closing_;
    }

    void wakeUp() const noexcept {
        const std::uint64_t one = 1;
        static_cast<void>(::write(wake_, &one, sizeof one));
    }

    void run(const std::stop_token& stop, std::promise<bool>& registered) {
        if (!connect()) {
            registered.set_value(false);
            return;
        }
        registered.set_value(true);
        while (!stop.stop_requested()) {
            std::array<pollfd, 3> watch{};
            nfds_t watched = 0;
            watch[watched++] = {.fd = sd_bus_get_fd(bus_.get()),
                                .events = static_cast<short>(sd_bus_get_events(bus_.get())),
                                .revents = 0};
            watch[watched++] = {.fd = wake_, .events = POLLIN, .revents = 0};
            if (pending_ && pending_->authentication.readable() >= 0) {
                watch[watched++] = {
                    .fd = pending_->authentication.readable(), .events = POLLIN, .revents = 0};
            }
            if (poll(watch.data(), watched, sdbus::pollTimeout(bus_.get())) < 0 && errno != EINTR) {
                lucent::warn("polkit", "the agent stopped: poll failed: {}", std::strerror(errno));
                break;
            }
            std::uint64_t woken = 0;
            static_cast<void>(::read(wake_, &woken, sizeof woken));
            int processed = 0;
            while ((processed = sd_bus_process(bus_.get(), nullptr)) > 0) {
            }
            if (processed < 0) {
                lucent::warn("polkit", "the agent stopped: the system bus failed: {}",
                             std::strerror(-processed));
                break;
            }
            takeInbox();
            if (pending_) {
                pending_->authentication.pump();
                settle();
            }
            publish();
        }
        if (pending_) {
            reply(cancelledError, "openSU is closing");
        }
        unregister();
    }

    bool connect() {
        sd_bus* opened = nullptr;
        if (const int code = sd_bus_open_system(&opened); code < 0) {
            lucent::warn("polkit", "no agent: the system bus cannot be reached: {}",
                         std::strerror(-code));
            return false;
        }
        bus_.reset(opened);
        sd_bus_slot* slot = nullptr;
        if (const int code = sd_bus_add_object_vtable(bus_.get(), &slot, agentPath, agentInterface,
                                                      agentVtable, this);
            code < 0) {
            lucent::warn("polkit", "no agent: its object cannot be put on the bus: {}",
                         std::strerror(-code));
            return false;
        }
        slot_.reset(slot);
        if (!findAuthority()) {
            return false;
        }
        BusError error;
        const int code = sd_bus_call_method(
            bus_.get(), authorityName, authorityPath, authorityInterface,
            "RegisterAuthenticationAgent", error.get(), nullptr, "(sa{sv})ss", "unix-process",
            subjectFields, "pid", "u", static_cast<std::uint32_t>(getpid()), "start-time", "t",
            startTime_, "uid", "i", static_cast<std::int32_t>(getuid()), locale_.c_str(),
            agentPath);
        if (code < 0) {
            lucent::warn("polkit", "no agent: polkit refused it: {}", error.text(code));
            return false;
        }
        lucent::info("polkit", "openSU's agent asks for what pkexec children of pid {} need",
                     getpid());
        return true;
    }

    /// polkit's unique bus name, the only caller the agent answers.
    bool findAuthority() {
        BusError error;
        sd_bus_message* reply = nullptr;
        const int code = sd_bus_call_method(
            bus_.get(), "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
            "GetNameOwner", error.get(), &reply, "s", authorityName);
        const Message owned{reply};
        const char* owner = nullptr;
        if (code < 0 || sd_bus_message_read(reply, "s", &owner) < 0) {
            lucent::warn("polkit", "no agent: polkit is not on the system bus: {}",
                         error.text(code));
            return false;
        }
        authority_ = owner;
        return true;
    }

    void unregister() {
        BusError error;
        const int code = sd_bus_call_method(
            bus_.get(), authorityName, authorityPath, authorityInterface,
            "UnregisterAuthenticationAgent", error.get(), nullptr, "(sa{sv})s", "unix-process",
            subjectFields, "pid", "u", static_cast<std::uint32_t>(getpid()), "start-time", "t",
            startTime_, "uid", "i", static_cast<std::int32_t>(getuid()), agentPath);
        if (code < 0) {
            lucent::warn("polkit", "the agent could not be unregistered: {}", error.text(code));
        }
    }

    [[nodiscard]] bool fromAuthority(sd_bus_message* message) const {
        const char* sender = sd_bus_message_get_sender(message);
        return sender != nullptr && authority_ == sender;
    }

    /// The player's answer and cancel, taken on the worker.
    void takeInbox() {
        std::string secret;
        std::uint64_t serial = 0;
        bool answered = false;
        bool cancelling = false;
        bool closing = false;
        {
            const std::lock_guard lock{mutex_};
            answered = std::exchange(hasAnswer_, false);
            secret.swap(answer_);
            serial = answerSerial_;
            cancelling = std::exchange(cancelWanted_, false);
            closing = closing_;
        }
        if (closing && pending_) {
            lucent::info("polkit", "openSU is closing; the authentication is cancelled");
            reply(cancelledError, "openSU is closing");
        } else if (cancelling && pending_) {
            lucent::info("polkit", "the player cancelled the authentication");
            reply(cancelledError, "the player cancelled it");
        } else if (answered && pending_) {
            pending_->authentication.answer(serial, secret);
        }
        secrets::wipe(secret);
    }

    /// Replies to polkit once the authentication has an outcome.
    void settle() {
        using Outcome = polkit::Authentication::Outcome;
        if (!pending_) {
            return;
        }
        switch (pending_->authentication.outcome()) {
        case Outcome::Pending:
            return;
        case Outcome::Authenticated:
            reply(nullptr, nullptr);
            return;
        case Outcome::Failed:
            reply(failedError, pending_->authentication.failure().c_str());
            return;
        }
    }

    /// Answers polkit's call: done when `name` is null, else that error.
    void reply(const char* name, const char* why) {
        if (!pending_) {
            return;
        }
        const int code = name == nullptr
                             ? sd_bus_reply_method_return(pending_->call.get(), "")
                             : sd_bus_reply_method_errorf(pending_->call.get(), name, "%s", why);
        if (code < 0) {
            lucent::warn("polkit", "polkit could not be answered: {}", std::strerror(-code));
        }
        pending_.reset();
    }

    void publish() {
        std::optional<polkit::Prompt> now;
        if (pending_) {
            now = pending_->authentication.prompt();
        }
        const std::lock_guard lock{mutex_};
        shown_ = withInbox(std::move(now));
    }

    /// `now` as the player sees it with their answer or cancel not yet taken. Under `mutex_`.
    [[nodiscard]] std::optional<polkit::Prompt> withInbox(std::optional<polkit::Prompt> now) const {
        if (cancelWanted_ || closing_) {
            return std::nullopt;
        }
        if (now && hasAnswer_ && now->serial == answerSerial_) {
            now->state = polkit::Prompt::State::Waiting;
        }
        return now;
    }

    fs::path helper_;
    std::string locale_;
    std::uint64_t startTime_;
    int wake_;
    // Worker only.
    Bus bus_;
    Slot slot_;
    std::string authority_;
    std::optional<Pending> pending_;
    std::uint64_t serials_{};
    // Shared with the shell, under `mutex_`.
    mutable std::mutex mutex_;
    std::optional<polkit::Prompt> shown_;
    std::string answer_;
    std::uint64_t answerSerial_{};
    bool hasAnswer_{false};
    bool cancelWanted_{false};
    bool closing_{false};
    // Last, so it stops before the members it uses go.
    std::jthread worker_;
};

int beginCall(sd_bus_message* message, void* agent, sd_bus_error* error) {
    return static_cast<SdBusPolkitAgent*>(agent)->begin(message, error);
}

int cancelCall(sd_bus_message* message, void* agent, sd_bus_error* error) {
    return static_cast<SdBusPolkitAgent*>(agent)->cancelled(message, error);
}

} // namespace

std::unique_ptr<PolkitAgent> startPolkitAgent(std::string locale, const fs::path& helperStandIn) {
    const std::optional<fs::path> helper =
        helperStandIn.empty() ? installedHelper() : std::optional<fs::path>{helperStandIn};
    if (!helper) {
        lucent::warn("polkit", "no agent: polkit-agent-helper-1 is not installed");
        return nullptr;
    }
    const std::optional<std::uint64_t> started = launch::procfs::startTime(getpid());
    if (!started) {
        lucent::warn("polkit", "no agent: this process's start time cannot be read");
        return nullptr;
    }
    auto agent = std::make_unique<SdBusPolkitAgent>(*helper, std::move(locale), *started);
    if (!agent->start()) {
        return nullptr;
    }
    return agent;
}

} // namespace opensu::host
