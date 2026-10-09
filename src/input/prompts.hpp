// prompts — what decides the caps and glyphs the prompts show: the device the player last used and
// the shortcut table that names the keys.
#pragma once

#include "last_device.hpp"
#include "shortcuts.hpp"

namespace opensu::input {

struct Prompts {
    LastDevice device;
    Shortcuts shortcuts;
};

} // namespace opensu::input
