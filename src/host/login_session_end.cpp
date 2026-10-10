#include "login_session_end.hpp"

#include <utility>

#include "lucent/log.h"
#include "session_mode.hpp"

namespace opensu::host {

LoginSessionEnd::LoginSessionEnd(Runner run, std::filesystem::path selector, DesktopRequest request,
                                 std::chrono::seconds quickWindow)
    : run_{std::move(run)}, selector_{std::move(selector)}, request_{std::move(request)},
      quickWindow_{quickWindow}, started_{std::chrono::steady_clock::now()} {
}

LoginSessionEnd::Ending LoginSessionEnd::settle(int status, bool stopRequested) {
    std::error_code error;
    const bool selectorInstalled = std::filesystem::is_regular_file(selector_, error);
    if (request_.consume()) {
        return selectorInstalled ? Ending::SwitchedToDesktop : Ending::StartDesktop;
    }
    const auto ranFor = std::chrono::steady_clock::now() - started_;
    if (stopRequested || (status == 0 && ranFor >= quickWindow_)) {
        return Ending::Ordinary;
    }
    lucent::error("session", "the session ended abnormally (status {}) after {}s", status,
                  std::chrono::duration_cast<std::chrono::seconds>(ranFor).count());
    if (!selectorInstalled) {
        lucent::warn("session",
                     "no {}: openSU did not change the autologin, so there is nothing to restore",
                     selector_.string());
        return Ending::NothingToRestore;
    }
    failure_ = runSelector(run_, selector_, "restore");
    if (!failure_.empty()) {
        lucent::error("session", "restoring the previous session failed: {}", failure_);
        return Ending::RestoreFailed;
    }
    lucent::warn("session", "the previous session is the next login again");
    return Ending::Restored;
}

} // namespace opensu::host
