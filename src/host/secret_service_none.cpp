#include "secret_service.hpp"

namespace opensu::host::secret_service {

bool built() noexcept {
    return false;
}

std::optional<std::string> lookup(std::string_view /*name*/, std::string& error) {
    error = "built without libsecret";
    return std::nullopt;
}

bool store(const SecretItem& /*item*/, std::string_view /*value*/, std::string& error) {
    error = "built without libsecret";
    return false;
}

bool clear(std::string_view /*name*/, std::string& /*error*/) {
    return true;
}

} // namespace opensu::host::secret_service
