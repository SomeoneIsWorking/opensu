// secret — clearing memory that held a password or key.
#pragma once

#include <cstddef>
#include <string>

namespace opensu::secrets {

/// Overwrites `size` bytes at `data` in a way the compiler keeps.
void wipeBytes(void* data, std::size_t size);

/// Overwrites everything `text` has allocated, so a password typed into it (and anything a
/// backspace left past its end) does not outlive the buffer, then empties it.
inline void wipe(std::string& text) {
    text.resize(text.capacity());
    wipeBytes(text.data(), text.size());
    text.clear();
}

} // namespace opensu::secrets
