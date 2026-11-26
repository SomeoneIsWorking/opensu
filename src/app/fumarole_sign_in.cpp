#include "fumarole_sign_in.hpp"

#include <system_error>
#include <vector>

#include "launch/command.hpp"
#include "lucent/log.h"

namespace opensu::app {
namespace {

/// A log line's own words, after fumarole's time and tag.
std::string wordsOf(std::string_view line) {
    const std::size_t tag = line.find("] [");
    const std::size_t end = tag == std::string_view::npos ? tag : line.find("] ", tag + 3);
    return std::string{end == std::string_view::npos ? line : line.substr(end + 2)};
}

} // namespace

void FumaroleSignIn::start(Way way) {
    cancel();
    {
        const std::scoped_lock lock{mutex_};
        result_.reset();
    }
    worker_ = std::jthread{[this, way](const std::stop_token& stop) {
        run(stop, way);
    }};
}

void FumaroleSignIn::cancel() {
    worker_ = {};
}

std::optional<FumaroleSignIn::Result> FumaroleSignIn::take() {
    const std::scoped_lock lock{mutex_};
    std::optional<Result> taken = std::move(result_);
    result_.reset();
    return taken;
}

void FumaroleSignIn::run(const std::stop_token& stop, Way way) {
    Result result;
    if (program_.empty()) {
        result.message = "fumarole is not installed";
    } else {
        std::error_code ignored;
        std::filesystem::create_directories(qr_.parent_path(), ignored);
        std::filesystem::remove(qr_, ignored);
        const std::vector<std::string> args =
            way == Way::Qr ? std::vector<std::string>{"auth", "--qr", qr_.string()}
                           : std::vector<std::string>{"auth", "--from-steam"};
        std::string last;
        const std::optional<int> status = launch::runStreaming(
            program_.string(), args,
            [&last](std::string_view line) {
                last = wordsOf(line);
            },
            stop);
        std::filesystem::remove(qr_, ignored);
        if (stop.stop_requested()) {
            return;
        }
        result.ok = status && *status == 0;
        result.message = result.ok ? "Signed in to Steam"
                         : !status ? "fumarole could not be started"
                                   : "Steam sign-in failed: " + last;
    }
    lucent::info("fumarole", "sign-in {}: {}", result.ok ? "done" : "failed", result.message);
    const std::scoped_lock lock{mutex_};
    result_ = std::move(result);
}

} // namespace opensu::app
