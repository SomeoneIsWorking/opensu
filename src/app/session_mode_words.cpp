#include "session_mode_words.hpp"

#include <string>

#include "host/session_mode.hpp"

namespace opensu::app {

RootInstallWords sessionModeWords() {
    return RootInstallWords{
        .cardId = "session.installing",
        .title = "Install session mode",
        .explanation =
            "Session mode starts openSU as its own login session: full screen on Gamescope, with "
            "no desktop under it. You come back with Power, then Switch to desktop.\n\n" +
            std::string{"This installs the session entry in "} +
            std::string{host::sddmSessionDirs.front()} + ", a small session selector at " +
            host::selectorPath + " and a sudo rule in " + host::sudoersRulePath +
            " that lets you run only that selector without a password.\n\n"
            "polkit then asks for the administrator password.",
        .installed = "Session mode installed. Power now has Switch to session mode"};
}

} // namespace opensu::app
