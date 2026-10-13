// A stand-in for opensu-retro's control channel: /status, save, load and reset over loopback, the
// requests it was sent recorded, and its port announced in a control file as the runner does.
#pragma once

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "lucent/http.h"

namespace opensu::test {

class FakeRunner {
  public:
    /// `refusal`, when set, is the reason a save is refused with 409.
    explicit FakeRunner(std::string refusal = {})
        : refusal_{std::move(refusal)}, server_{{}, [this](const lucent::http::Request& request) {
                                                    return handle(request);
                                                }} {
        if (!server_.start()) {
            std::fprintf(stderr, "FAIL: the fake runner cannot listen\n");
            std::exit(1);
        }
    }

    /// Writes the port to the file a launch's `NAME=value` environment entry names.
    void announce(std::string_view entry) const {
        const std::filesystem::path file{std::string{entry.substr(entry.find('=') + 1)}};
        std::filesystem::create_directories(file.parent_path());
        std::ofstream{file} << nlohmann::json{{"port", server_.port()}, {"pid", 1}}.dump();
    }

    void stop() {
        server_.stop();
    }

    [[nodiscard]] std::vector<std::string> calls() const {
        const std::lock_guard lock{mutex_};
        return calls_;
    }

    std::atomic<bool> hasState{false};

  private:
    lucent::http::Response handle(const lucent::http::Request& request) {
        const std::string path{request.path()};
        if (path == "/status") {
            const nlohmann::json status{{"core", "fake"},
                                        {"rom", "/roms/game.gba"},
                                        {"frame", 42},
                                        {"hasState", hasState.load()}};
            return lucent::http::Response::json(200, "OK", status.dump());
        }
        {
            const std::lock_guard lock{mutex_};
            calls_.push_back(path);
        }
        if (path == "/state/save" && !refusal_.empty()) {
            return lucent::http::Response::text(409, "Conflict", refusal_);
        }
        if (path == "/state/save") {
            hasState.store(true);
        }
        if (path == "/state/load" && !hasState.load()) {
            return lucent::http::Response::text(404, "Not Found",
                                                "no state is saved for this game");
        }
        return lucent::http::Response::text(200, "OK", "done");
    }

    std::string refusal_;
    mutable std::mutex mutex_;
    std::vector<std::string> calls_;
    lucent::http::Server server_;
};

} // namespace opensu::test
