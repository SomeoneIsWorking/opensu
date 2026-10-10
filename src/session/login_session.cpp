#include "login_session.hpp"

#include <utility>

#include "host/desktop_request.hpp"
#include "lucent/log.h"

namespace opensu::session {

LoginSession::LoginSession(CompositorSession compositor, host::LoginSessionEnd end,
                           std::vector<std::filesystem::path> executablePath)
    : compositor_{std::move(compositor)}, end_{std::move(end)},
      executablePath_{std::move(executablePath)} {
}

int LoginSession::run(const std::vector<std::string>& args) {
    const int status = compositor_.run(Output{}, args);
    using Ending = host::LoginSessionEnd::Ending;
    switch (end_.settle(status, compositor_.stopRequested())) {
    case Ending::StartDesktop:
        lucent::info("session", "starting the desktop");
        lucent::error("session", "{}", host::DesktopRequest::enterDesktop(executablePath_));
        return 1;
    case Ending::RestoreFailed:
    case Ending::Restored:
    case Ending::NothingToRestore:
        return status == 0 ? 1 : status;
    case Ending::SwitchedToDesktop:
    case Ending::Ordinary:
        break;
    }
    return status;
}

} // namespace opensu::session
