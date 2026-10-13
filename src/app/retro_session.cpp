#include "retro_session.hpp"

#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>
#include <unistd.h>

#include "lucent/http_client.h"
#include "lucent/log.h"

namespace opensu::app {
namespace {

/// The control file as JSON; null when it is missing or not whole.
nlohmann::json readControl(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        return nullptr;
    }
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    return nlohmann::json::parse(text, nullptr, false);
}

} // namespace

RetroSession::RetroSession(std::filesystem::path directory) : directory_{std::move(directory)} {
}

std::string RetroSession::begin() {
    const std::lock_guard lock{mutex_};
    file_ = directory_ /
            ("retro-" + std::to_string(getpid()) + "-" + std::to_string(launches_++) + ".json");
    port_.reset();
    std::error_code error;
    std::filesystem::remove(file_, error);
    return std::string{variable} + "=" + file_.string();
}

std::string RetroSession::end() {
    const std::lock_guard lock{mutex_};
    if (file_.empty()) {
        return {};
    }
    const nlohmann::json control = readControl(file_);
    std::string failure;
    if (control.is_object() && control.contains("error") && control["error"].is_string()) {
        failure = control["error"].get<std::string>();
    }
    std::error_code error;
    std::filesystem::remove(file_, error);
    file_.clear();
    port_.reset();
    return failure;
}

std::optional<std::uint16_t> RetroSession::port() {
    const std::lock_guard lock{mutex_};
    if (!port_ && !file_.empty()) {
        const nlohmann::json control = readControl(file_);
        if (control.is_object() && control.contains("port") &&
            control["port"].is_number_unsigned()) {
            port_ = control["port"].get<std::uint16_t>();
        }
    }
    return port_;
}

std::optional<RetroSession::Status> RetroSession::status() {
    const std::optional<std::uint16_t> at = port();
    if (!at) {
        return std::nullopt;
    }
    lucent::http::FetchResult result;
    std::string error;
    if (!lucent::http::get_loopback(*at, "/status", result, error) || result.status != 200) {
        return std::nullopt;
    }
    const nlohmann::json body = nlohmann::json::parse(result.body, nullptr, false);
    if (!body.is_object()) {
        return std::nullopt;
    }
    return Status{.core = body.value("core", std::string{}),
                  .rom = body.value("rom", std::string{}),
                  .frame = body.value("frame", std::uint64_t{0}),
                  .hasState = body.value("hasState", false)};
}

std::string RetroSession::post(std::string_view target) {
    const std::optional<std::uint16_t> at = port();
    if (!at) {
        return "the game has no control channel";
    }
    lucent::http::FetchResult result;
    std::string error;
    if (!lucent::http::post_loopback(*at, target, "", result, error)) {
        lucent::warn("retro", "POST {} to the runner failed: {}", target, error);
        return "the game does not answer";
    }
    if (result.status != 200) {
        return result.body.empty() ? "the game refused (" + std::to_string(result.status) + ")"
                                   : result.body;
    }
    return {};
}

std::string RetroSession::saveState() {
    return post("/state/save");
}

std::string RetroSession::loadState() {
    return post("/state/load");
}

std::string RetroSession::reset() {
    return post("/reset");
}

} // namespace opensu::app
