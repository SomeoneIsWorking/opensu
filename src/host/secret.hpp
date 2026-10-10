// secret — clearing a buffer that held a password.
#pragma once

#include <cstring>
#include <string>

namespace opensu::host {

/// Overwrites everything `text` has allocated, so a password typed into it (and anything a
/// backspace left past its end) does not outlive the buffer, then empties it. The write is not
/// optimised away.
inline void wipe(std::string& text) {
    text.resize(text.capacity());
    explicit_bzero(text.data(), text.size());
    text.clear();
}

} // namespace opensu::host
