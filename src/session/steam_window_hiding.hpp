// steam_window_hiding — asks openSU's Gamescope fork to leave Steam's own client windows out of
// focus, the composite and GAMESCOPE_FOCUSABLE_WINDOWS, through GAMESCOPE_HIDE_STEAM_CLIENT
// (CARDINAL 0/1) on the root. Steam's windows are told apart by the fork (`steam` and
// `steamwebhelper` processes, app id 769, STEAM_BIGPICTURE); the games Steam starts still show.
#pragma once

#include "root_properties.hpp"

namespace opensu::session {

class SteamWindowHiding {
  public:
    /// Opens a connection to the X display opensu's window lives on.
    /// Throws std::runtime_error when the display cannot be opened.
    SteamWindowHiding();

    void hide(bool hides);

  private:
    RootProperties root_;
};

} // namespace opensu::session
