#include "client.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include "launch/command.hpp"
#include "library/steam.hpp"
#include "lucent/log.h"

namespace opensu::steam {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using launch::SteamState;

/// How often the watcher looks while Steam is coming up, then once it is up.
constexpr auto startingPoll = std::chrono::milliseconds{500};
constexpr auto runningPoll = std::chrono::milliseconds{1000};
/// How often a waiter checks for cancellation.
constexpr auto cancelPoll = std::chrono::milliseconds{100};
constexpr auto shutdownCommandWait = std::chrono::milliseconds{30000};
constexpr auto exitPoll = std::chrono::milliseconds{200};

/// Both halves of the line Steam logs when its logon has finished.
bool isLogonLine(const std::string& line) {
    return line.find("[Logged On") != std::string::npos &&
           line.find("RecvMsgClientLogOnResponse() : processing complete") != std::string::npos;
}

} // namespace

Client::Client(Options options)
    : options_{std::move(options)}, desktop_{options_.home},
      connectionLog_{options_.home / ".steam" / "steam" / "logs" / "connection_log.txt"} {
}

Client::~Client() {
    shutdown();
}

SteamState Client::state() const {
    const std::lock_guard lock{mutex_};
    return state_;
}

void Client::setState(SteamState state) {
    {
        const std::lock_guard lock{mutex_};
        state_ = state;
    }
    changed_.notify_all();
}

void Client::start() {
    if (state() != SteamState::Stopped) {
        return;
    }
    if (desktop_.runningOutside("")) {
        lucent::warn("steam", "a Steam client already runs outside opensu");
        setState(SteamState::Blocked);
        return;
    }
    program_ = launch::resolveExecutable("steam", options_.executablePath);
    if (program_.empty()) {
        lucent::error("steam", "steam is not on PATH");
        setState(SteamState::Failed);
        return;
    }

    const fs::path privateBus =
        launch::resolveExecutable("dbus-run-session", options_.executablePath);
    if (privateBus.empty()) {
        lucent::error("steam",
                      "dbus-run-session is not on PATH; install it with `sudo dnf install "
                      "dbus-daemon`, `sudo apt install dbus-daemon` or `sudo pacman -S dbus`");
        setState(SteamState::Failed);
        return;
    }

    std::error_code ec;
    const std::uintmax_t size = fs::file_size(connectionLog_, ec);
    logOffset_ = ec ? 0 : size;

    // A bus of its own keeps Steam's tray icon and notifications off the desktop's panel;
    // -applaunch and -shutdown reach it through its pipe, not the bus. DevTools carries the
    // download queue and the installer.
    std::string failure;
    if (!instance_.start(options_.session + "-steam.scope", privateBus.string(),
                         {"--", program_.string(), "-silent", "-cef-enable-debugging"}, failure)) {
        lucent::error("steam", "{}", failure);
        setState(SteamState::Failed);
        return;
    }
    {
        const std::lock_guard lock{mutex_};
        state_ = SteamState::Initializing;
        stopping_ = false;
    }
    watcher_ = std::thread{[this] {
        watch();
    }};
}

bool Client::logonCompleted() {
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(connectionLog_, ec);
    if (ec) {
        return false;
    }
    if (size < logOffset_) {
        // Rotated or truncated: everything in it is newer than what was seen.
        logOffset_ = 0;
    }
    std::ifstream in{connectionLog_, std::ios::binary};
    in.seekg(static_cast<std::streamoff>(logOffset_));
    const std::string appended{std::istreambuf_iterator<char>{in},
                               std::istreambuf_iterator<char>{}};

    // A line is only read once complete, so one written in two pieces is not missed.
    const std::size_t end = appended.rfind('\n');
    if (end == std::string::npos) {
        return false;
    }
    logOffset_ += end + 1;
    std::size_t start = 0;
    while (start < end) {
        const std::size_t stop = appended.find('\n', start);
        if (isLogonLine(appended.substr(start, stop - start))) {
            return true;
        }
        start = stop + 1;
    }
    return false;
}

void Client::watch() {
    std::unique_lock lock{mutex_};
    while (!stopping_) {
        const SteamState current = state_;
        lock.unlock();
        const bool alive = instance_.running();
        const bool ready = alive && current == SteamState::Initializing && logonCompleted();
        if (alive && (ready || current == SteamState::Ready)) {
            refreshFromClient();
        }
        lock.lock();
        if (stopping_) {
            break;
        }
        if (!alive) {
            lucent::error("steam", "the Steam client is gone");
            state_ = SteamState::Failed;
            changed_.notify_all();
            break;
        }
        if (ready) {
            lucent::info("steam", "logged on");
            state_ = SteamState::Ready;
            changed_.notify_all();
        }
        const auto interval = state_ == SteamState::Initializing ? startingPoll : runningPoll;
        changed_.wait_for(lock, interval, [this] {
            return stopping_;
        });
    }
}

bool Client::installed(std::string_view appId) const {
    return library::steam::Library::discover(options_.home, options_.steamRoots).installed(appId);
}

void Client::refreshFromClient() {
    std::string error;
    std::optional<std::vector<Download>> queue = queue_.read(error);
    std::optional<Activities> activities;
    if (queue) {
        activities = launches_.read(error);
    }
    const std::lock_guard lock{mutex_};
    if (!queue || !activities) {
        if (error != queueError_) {
            lucent::warn("steam", "cannot read Steam's downloads and launches: {}", error);
            queueError_ = error;
        }
        return;
    }
    queueError_.clear();
    downloads_ = std::move(*queue);
    activities_ = std::move(*activities);
}

launch::SteamAppActivity Client::activity(std::string_view appId) const {
    const std::lock_guard lock{mutex_};
    const auto found = activities_.find(appId);
    return found == activities_.end() ? launch::SteamAppActivity{} : found->second;
}

std::vector<Download> Client::downloads() const {
    const std::lock_guard lock{mutex_};
    return downloads_;
}

std::optional<double> Client::updateProgress(std::string_view appId) const {
    const std::lock_guard lock{mutex_};
    for (const Download& download : downloads_) {
        if (download.appId == appId) {
            return download.progress;
        }
    }
    return std::nullopt;
}

SteamState Client::waitReady(std::chrono::milliseconds timeout,
                             const std::function<bool()>& cancelled) {
    const Clock::time_point until = Clock::now() + timeout;
    std::unique_lock lock{mutex_};
    while (state_ == SteamState::Initializing && !cancelled() && Clock::now() < until) {
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(until - Clock::now());
        changed_.wait_for(lock, std::min(remaining, cancelPoll));
    }
    return state_;
}

void Client::shutdown() {
    {
        const std::lock_guard lock{mutex_};
        stopping_ = true;
    }
    changed_.notify_all();
    if (watcher_.joinable()) {
        watcher_.join();
    }

    if (instance_.running()) {
        lucent::info("steam", "asking Steam to exit");
        const auto asked =
            launch::runCommand(program_.string(), {"-shutdown"}, shutdownCommandWait);
        const Clock::time_point until = Clock::now() + shutdownWait;
        while (asked && instance_.running() && Clock::now() < until) {
            std::this_thread::sleep_for(exitPoll);
        }
    }
    instance_.stop();
    {
        const std::lock_guard lock{mutex_};
        downloads_.clear();
        activities_.clear();
    }
    setState(SteamState::Stopped);
}

} // namespace opensu::steam
