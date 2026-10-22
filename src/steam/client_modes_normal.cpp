#include "client_modes.hpp"

#include <array>

namespace opensu::steam {
namespace {

constexpr std::array modes{Client::Mode::Normal};

} // namespace

std::span<const Client::Mode> clientModes() noexcept {
    return modes;
}

} // namespace opensu::steam
