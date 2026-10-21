#include "secret_bus.hpp"

#include <thread>

namespace opensu::steam {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr auto socketPoll = std::chrono::milliseconds{20};

} // namespace

bool SecretBus::start(const std::string& unit, const fs::path& proxy, const std::string& desktopBus,
                      const fs::path& socket, std::string& failure) {
    std::error_code error;
    fs::create_directories(socket.parent_path(), error);
    fs::remove(socket, error);
    socket_ = socket;
    if (!instance_.start(
            unit, proxy.string(),
            {desktopBus, socket.string(), "--filter", "--talk=org.freedesktop.secrets"}, failure)) {
        return false;
    }
    const Clock::time_point until = Clock::now() + secretBusWait;
    while (!fs::is_socket(socket, error)) {
        if (Clock::now() >= until || !instance_.running()) {
            failure = "the Secret Service proxy did not listen at " + socket.string();
            stop();
            return false;
        }
        std::this_thread::sleep_for(socketPoll);
    }
    return true;
}

void SecretBus::stop() {
    instance_.stop();
    if (!socket_.empty()) {
        std::error_code error;
        fs::remove(socket_, error);
        socket_.clear();
    }
}

} // namespace opensu::steam
