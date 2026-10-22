#include "secret_service.hpp"

#include <memory>

#include <libsecret/secret.h>

#include "secret.hpp"

namespace opensu::host::secret_service {
namespace {

// Items are found by openSU's schema name and their `name` attribute.
constexpr SecretSchema openSuSchema{
    "io.github.opensu.Secret",
    SECRET_SCHEMA_NONE,
    {{"name", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING}},
    0,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr};

struct ErrorFree {
    void operator()(GError* error) const noexcept {
        g_error_free(error);
    }
};
using ErrorHandle = std::unique_ptr<GError, ErrorFree>;

bool failed(GError* raw, std::string& error) {
    const ErrorHandle owned{raw};
    if (owned == nullptr) {
        return false;
    }
    error = std::string{"the keyring: "} + owned->message;
    return true;
}

} // namespace

bool built() noexcept {
    return true;
}

std::optional<std::string> lookup(std::string_view name, std::string& error) {
    const std::string key{name};
    GError* raw = nullptr;
    gchar* found =
        secret_password_lookup_sync(&openSuSchema, nullptr, &raw, "name", key.c_str(), nullptr);
    if (failed(raw, error) || found == nullptr) {
        return std::nullopt;
    }
    std::string value{found};
    secret_password_free(found);
    return value;
}

bool store(const SecretItem& item, std::string_view value, std::string& error) {
    const std::string key{item.name};
    const std::string shown{item.label};
    std::string secret{value};
    GError* raw = nullptr;
    const gboolean stored =
        secret_password_store_sync(&openSuSchema, SECRET_COLLECTION_DEFAULT, shown.c_str(),
                                   secret.c_str(), nullptr, &raw, "name", key.c_str(), nullptr);
    wipe(secret);
    return !failed(raw, error) && stored == TRUE;
}

bool clear(std::string_view name, std::string& error) {
    const std::string key{name};
    GError* raw = nullptr;
    secret_password_clear_sync(&openSuSchema, nullptr, &raw, "name", key.c_str(), nullptr);
    return !failed(raw, error);
}

} // namespace opensu::host::secret_service
