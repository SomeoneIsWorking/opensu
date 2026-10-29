// secret — clearing memory that held a password or key.
#pragma once

#include <cstddef>
#include <string>

namespace opensu::secrets {

/// Overwrites `size` bytes at `data` in a way the compiler keeps.
void wipeBytes(void* data, std::size_t size) noexcept;

/// Overwrites everything `text` has allocated, so a password typed into it (and anything a
/// backspace left past its end) does not outlive the buffer, then empties it.
inline void wipe(std::string& text) noexcept {
    // The string's buffer holds capacity() chars past data(), whatever its size.
    wipeBytes(text.data(), text.capacity());
    text.clear();
}

} // namespace opensu::secrets
