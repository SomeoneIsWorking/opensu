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
    : options_{std::move(options)}, desktop_{options_.home}, orphans_{options_.home},
      connectionLog_{options_.home / ".steam" / "steam" / "logs" / "connection_log.txt"} {
}

Client::~Client() {
    shutdown();
}

SteamState Client::state() const {
    const std::lock_guard lock{mutex_};
    return state_;
}

void Client::ensureStarted() {
    std::optional<Mode> mode;
    SandboxDevices devices;
    {
        const std::lock_guard lock{mutex_};
        // Nothing to do while it is up; Normal is not openSU's to start.
        if (state_ != SteamState::Stopped || mode_ == Mode::Normal) {
            return;
        }
        mode = mode_;
        devices = devices_;
    }
    lucent::info("steam", "a launch needs Steam; starting it in its mode");
    start(*mode, devices);
}

bool Client::ownsClient() const {
    return mode() != Mode::Normal;
}

void Client::setState(SteamState state) {
    {
        const std::lock_guard lock{mutex_};
        state_ = state;
    }
    changed_.notify_all();
}

Client::Mode Client::mode() const {
    const std::lock_guard lock{mutex_};
    return mode_;
}

bool Client::showsInSession() const {
    return mode() != Mode::Normal;
}

void Client::start(Mode mode, const SandboxDevices& devices) {
    if (state() != SteamState::Stopped) {
        return;
    }
    {
        const std::lock_guard lock{mutex_};
        mode_ = mode;
        devices_ = devices;
    }
    if (mode == Mode::Normal) {
        startNormal();
    } else {
        startOwn(mode, devices);
    }
}

void Client::beginWatching(SteamState state) {
    {
        const std::lock_guard lock{mutex_};
        state_ = state;
        stopping_ = false;
    }
    changed_.notify_all();
    watcher_ = std::thread{[this] {
        watch();
    }};
}

void Client::startNormal() {
    // Normal is a launcher: attach to a running Steam, start nothing.
    if (!options_.mayStart) {
        lucent::info("steam", "Normal mode: this run starts and attaches to no Steam");
        return;
    }
    const std::optional<pid_t> running = desktop_.running();
    if (running) {
        lucent::info("steam", "Normal mode: attached to the desktop Steam (pid {})", *running);
        beginWatching(SteamState::Ready);
        return;
    }
    lucent::info("steam", "Normal mode: no desktop Steam runs; a launch will start it");
}

bool Client::sandbox(const SandboxDevices& devices, std::string& command,
                     std::vector<std::string>& args, std::filesystem::path& proxy) const {
    const std::filesystem::path bwrap = launch::resolveExecutable("bwrap", options_.executablePath);
    if (bwrap.empty()) {
        lucent::error("steam", "Isolated Steam needs bwrap: install bubblewrap (Fedora: sudo dnf "
                               "install bubblewrap; Debian, Ubuntu: sudo apt install bubblewrap; "
                               "Arch: sudo pacman -S bubblewrap)");
        return false;
    }
    proxy = launch::resolveExecutable("xdg-dbus-proxy", options_.executablePath);
    if (proxy.empty()) {
        lucent::error("steam", "Isolated Steam needs xdg-dbus-proxy for its login: install it "
                               "(Fedora: sudo dnf install xdg-dbus-proxy; Debian, Ubuntu: sudo "
                               "apt install xdg-dbus-proxy; Arch: sudo pacman -S xdg-dbus-proxy)");
        return false;
    }
    std::string failure;
    std::vector<std::string> wrapped =
        sandboxArguments(options_.sandbox, devices, options_.secretBusSocket, failure);
    if (wrapped.empty()) {
        lucent::error("steam", "{}", failure);
        return false;
    }
    wrapped.push_back(command);
    wrapped.insert(wrapped.end(), args.begin(), args.end());
    command = bwrap.string();
    args = std::move(wrapped);
    return true;
}

void Client::startOwn(Mode mode, const SandboxDevices& devices) {
    const bool isolated = mode == Mode::Isolated;
    if (!options_.mayStart) {
        lucent::info("steam", "{} mode: this run starts no Steam client",
                     isolated ? "Isolated" : "Managed");
        return;
    }
    program_ = launch::resolveExecutable("steam", options_.executablePath);
    if (program_.empty()) {
        lucent::error("steam", "steam is not on PATH");
        setState(SteamState::Failed);
        return;
    }
    std::string command = program_.string();
    std::vector<std::string> args{"-silent", "-cef-enable-debugging"};
    std::filesystem::path proxy;
    if (isolated && !sandbox(devices, command, args, proxy)) {
        setState(SteamState::Failed);
        return;
    }

    orphans_.stop(program_, shutdownWait);
    if (desktop_.runningOutside("")) {
        lucent::warn("steam", "a Steam client already runs outside opensu");
        setState(SteamState::Blocked);
        return;
    }
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(connectionLog_, ec);
    logOffset_ = ec ? 0 : size;

    std::string failure;
    if (isolated && !secretBus_.start(options_.session + "-steam-bus.scope", proxy,
                                      options_.desktopBus, options_.secretBusSocket, failure)) {
        lucent::error("steam", "{}", failure);
        setState(SteamState::Failed);
        return;
    }
    // Managed: Steam's tray icon needs the desktop's StatusNotifierWatcher: on a bus of its own
    // Steam falls back to XEmbed, which the desktop's proxy shows as an icon that ignores clicks.
    const std::string scope = options_.session + "-steam.scope";
    if (!instance_.start(scope, command, args, failure)) {
        lucent::error("steam", "{}", failure);
        secretBus_.stop();
        setState(SteamState::Failed);
        return;
    }
    if (isolated) {
        lucent::info("steam", "Isolated mode: started Steam in {} in a sandbox with {}", scope,
                     devices.allInput
                         ? std::string{"every input node"}
                         : std::to_string(devices.inputNodes.size()) + " virtual pad nodes");
    } else {
        lucent::info("steam", "Managed mode: started Steam in {}", scope);
    }
    beginWatching(SteamState::Initializing);
}

bool Client::clientAlive() {
    if (mode() != Mode::Normal) {
        return instance_.running();
    }
    // A desktop Steam being started has not written its pid file yet.
    return desktop_.running().has_value() || state() == SteamState::Initializing;
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
        const bool alive = clientAlive();
        const bool ready = alive && current == SteamState::Initializing && logonCompleted();
        if (alive && (ready || current == SteamState::Ready)) {
            refreshFromClient();
        }
        lock.lock();
        if (stopping_) {
            break;
        }
        if (!alive) {
            lucent::error("steam", "the {} Steam client is gone",
                          mode_ == Mode::Normal     ? "desktop"
                          : mode_ == Mode::Isolated ? "isolated"
                                                    : "managed");
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

bool Client::hasManifest(std::string_view appId) const {
    return library::steam::Library::discover(options_.home, options_.steamRoots)
        .manifestOf(appId)
        .has_value();
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

void Client::cancelLaunch(std::string_view appId) {
    std::string error;
    if (!launches_.cancel(appId, error)) {
        lucent::warn("steam", "could not cancel the launch of {}: {}", appId, error);
        return;
    }
    lucent::info("steam", "cancelled the launch of {}", appId);
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
    secretBus_.stop();
    {
        const std::lock_guard lock{mutex_};
        downloads_.clear();
        activities_.clear();
    }
    setState(SteamState::Stopped);
}

} // namespace opensu::steam
