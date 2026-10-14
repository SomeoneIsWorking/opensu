#include "big_picture.hpp"

#include <string>

#include "lucent/log.h"

namespace opensu::steam {

bool BigPicture::close() {
    std::string error;
    if (!devTools_.evaluate("SteamClient.UI.ExitBigPictureMode()", error)) {
        lucent::warn("steam", "Steam would not leave Big Picture: {}", error);
        return false;
    }
    lucent::info("steam", "closed Big Picture");
    return true;
}

} // namespace opensu::steam
