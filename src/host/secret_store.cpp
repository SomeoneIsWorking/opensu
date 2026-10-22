#include "secret_store.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

#include "fileio/owner_only_write.hpp"
#include "secret_service.hpp"

namespace opensu::host {
namespace {

namespace fs = std::filesystem;

} // namespace

bool secretServiceBuilt() noexcept {
    return secret_service::built();
}

SecretStore::SecretStore(fs::path dir, bool keyring) : dir_{std::move(dir)}, keyring_{keyring} {
}

fs::path SecretStore::fileOf(std::string_view name) const {
    return dir_ / std::string{name};
}

bool SecretStore::usesKeyring() const noexcept {
    return keyring_ && secret_service::built();
}

std::optional<SecretStore::Found> SecretStore::read(std::string_view name,
                                                    std::string& error) const {
    if (usesKeyring()) {
        if (std::optional<std::string> kept = secret_service::lookup(name, error)) {
            return Found{std::move(*kept), SecretPlace::Keyring};
        }
    }
    const fs::path file = fileOf(name);
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) {
        return std::nullopt;
    }
    std::ifstream in{file, std::ios::binary};
    std::string value{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    if (!in.good() && !in.eof()) {
        error = "cannot read " + file.string();
        return std::nullopt;
    }
    return Found{std::move(value), SecretPlace::File};
}

std::optional<SecretPlace> SecretStore::save(std::string_view name, std::string_view label,
                                             std::string_view value, std::string& error) const {
    std::string keyringError;
    if (usesKeyring() && secret_service::store({name, label}, value, keyringError)) {
        std::error_code ec;
        fs::remove(fileOf(name), ec);
        return SecretPlace::Keyring;
    }
    if (!fileio::writeOwnerOnly(fileOf(name), value, error)) {
        if (!keyringError.empty()) {
            error = keyringError + "; " + error;
        }
        return std::nullopt;
    }
    return SecretPlace::File;
}

bool SecretStore::forget(std::string_view name, std::string& error) const {
    bool cleared = !usesKeyring() || secret_service::clear(name, error);
    std::error_code ec;
    fs::remove(fileOf(name), ec);
    if (ec) {
        error = "cannot remove " + fileOf(name).string() + ": " + ec.message();
        cleared = false;
    }
    return cleared;
}

} // namespace opensu::host
