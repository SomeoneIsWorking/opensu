#include "steam_window_hiding.hpp"

#include "lucent/log.h"

namespace opensu::session {

SteamWindowHiding::SteamWindowHiding() : root_{"hide Steam's windows"} {
}

void SteamWindowHiding::hide(bool hides) {
    const long value = hides ? 1 : 0;
    root_.setCardinal("GAMESCOPE_HIDE_STEAM_CLIENT", value);
    root_.flush();
    lucent::info("session", "GAMESCOPE_HIDE_STEAM_CLIENT {}: Steam's windows {} in Gamescope",
                 value, hides ? "hidden" : "shown");
}

} // namespace opensu::session
