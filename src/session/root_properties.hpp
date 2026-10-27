// root_properties — writes properties on the root window of the X display openSU's window lives
// on, which is how openSU asks its Gamescope fork for things (GAMESCOPE_HIDE_STEAM_CLIENT, the
// GAMESCOPE_FRAMEGEN_* atoms, GAMESCOPE_COLOR_NIGHT_MODE). One connection per owner; a write is
// flushed at once.
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace opensu::session {

class RootProperties {
  public:
    /// Opens a connection to the X display; `purpose` completes the error's "cannot open the X
    /// display to ...". Throws std::runtime_error when it cannot be opened.
    explicit RootProperties(std::string_view purpose);
    ~RootProperties();

    RootProperties(const RootProperties&) = delete;
    RootProperties& operator=(const RootProperties&) = delete;
    RootProperties(RootProperties&&) = delete;
    RootProperties& operator=(RootProperties&&) = delete;

    /// Sets `name` to one CARDINAL.
    void setCardinal(std::string_view name, long value);
    /// Sets `name` to a CARDINAL array.
    void setCardinals(std::string_view name, std::span<const std::uint32_t> values);
    /// Sets `name` to UTF8_STRING `value`.
    void setText(std::string_view name, std::string_view value);
    /// Sends what was set.
    void flush();

  private:
    struct Connection;
    std::unique_ptr<Connection> connection_;
};

} // namespace opensu::session
