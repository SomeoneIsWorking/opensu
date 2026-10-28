#include "steam_request.hpp"

#include "lucent/log.h"
#include "win32.hpp"

#include <shellapi.h>

namespace opensu::launch {

bool requestSteamRun(const library::Game& game,
                     const std::vector<std::filesystem::path>& /*executablePath*/,
                     std::string& failure) {
    const std::wstring link = win32::wide("steam://rungameid/" + game.sourceId);
    SHELLEXECUTEINFOW request{};
    request.cbSize = sizeof(request);
    request.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    request.lpVerb = L"open";
    request.lpFile = link.c_str();
    request.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&request) == 0) {
        lucent::warn("launch", "steam://rungameid/{} was not opened: error {}", game.sourceId,
                     win32::lastError());
        failure = "could not ask Steam to start " + game.title;
        return false;
    }
    lucent::info("launch", "asked Steam to run {} through steam://rungameid/{}", game.title,
                 game.sourceId);
    return true;
}

} // namespace opensu::launch
