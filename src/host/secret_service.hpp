// secret_service — the desktop's Secret Service (org.freedesktop.secrets) through libsecret, one
// backend per build: `secret_service_libsecret.cpp` with libsecret, `secret_service_none.cpp`
// without. Items carry openSU's schema and a `name` attribute.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace opensu::host::secret_service {

/// Whether this build has a backend at all.
[[nodiscard]] bool built() noexcept;

/// The item named `name`; nothing when there is none or the service did not answer (`error` set
/// then).
[[nodiscard]] std::optional<std::string> lookup(std::string_view name, std::string& error);

/// An item: the name it is found by, and the label the keyring's own manager shows.
struct SecretItem {
    std::string_view name;
    std::string_view label;
};

/// Stores `value` as `item`.
[[nodiscard]] bool store(const SecretItem& item, std::string_view value, std::string& error);

/// Removes the item `name`; true when there was none.
[[nodiscard]] bool clear(std::string_view name, std::string& error);

} // namespace opensu::host::secret_service
