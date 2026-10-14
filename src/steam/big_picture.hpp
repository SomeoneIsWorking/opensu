// big_picture — closes Steam's Big Picture through the running client, with
// `SteamClient.UI.ExitBigPictureMode()`, what Big Picture's own window calls on close
// (`steamui/chunk~2dcc5aaf7.js`). openSU is the controller-first screen; Big Picture is not wanted.
#pragma once

#include "devtools.hpp"

namespace opensu::steam {

class BigPicture {
  public:
    explicit BigPicture(DevTools& devTools) : devTools_{devTools} {
    }

    /// Asks Steam to leave Big Picture; false, logged, when Steam would not take the request.
    bool close();

  private:
    DevTools& devTools_;
};

} // namespace opensu::steam
