#include "fumarole_sign_in.hpp"

#include <system_error>
#include <utility>
#include <vector>

#include "launch/command.hpp"
#include "lucent/log.h"
#include "secrets/secret.hpp"

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
    begin(way == Way::Qr ? std::vector<std::string>{"auth", "--qr", qr_.string()}
                         : std::vector<std::string>{"auth", "--from-steam"},
          {});
}

void FumaroleSignIn::copy(std::string login) {
    begin({"auth", "--import"}, std::move(login));
}

void FumaroleSignIn::begin(std::vector<std::string> args, std::string input) {
    cancel();
    {
        const std::scoped_lock lock{mutex_};
        result_.reset();
    }
    worker_ = std::jthread{[this, args = std::move(args),
                            input = std::move(input)](const std::stop_token& stop) mutable {
        run(stop, args, input);
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

void FumaroleSignIn::run(const std::stop_token& stop, const std::vector<std::string>& args,
                         std::string& input) {
    Result result;
    if (program_.empty()) {
        result.message = "fumarole is not installed";
    } else {
        std::error_code ignored;
        std::filesystem::create_directories(qr_.parent_path(), ignored);
        std::filesystem::remove(qr_, ignored);
        std::string last;
        const std::optional<int> status = launch::runStreaming(
            program_.string(), args,
            [&last](std::string_view line) {
                last = wordsOf(line);
            },
            stop, input);
        secrets::wipe(input);
        std::filesystem::remove(qr_, ignored);
        if (stop.stop_requested()) {
            return;
        }
        result.ok = status && *status == 0;
        result.message = result.ok ? "Signed in to Steam"
                         : !status ? "fumarole could not be started"
                                   : "Steam sign-in failed: " + last;
    }
    secrets::wipe(input);
    lucent::info("fumarole", "sign-in {}: {}", result.ok ? "done" : "failed", result.message);
    const std::scoped_lock lock{mutex_};
    result_ = std::move(result);
}

} // namespace opensu::app
