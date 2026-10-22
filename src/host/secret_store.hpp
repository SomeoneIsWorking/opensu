// secret_store — a secret openSU keeps between runs (the SteamGridDB API key): in the desktop's
// Secret Service (the keyring) when the build has libsecret and the service answers, else in an
// owner-only file under the config dir. A secret never reaches a log line or an error text.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace opensu::host {

/// Where a secret is kept.
enum class SecretPlace : std::uint8_t {
    Keyring,
    File,
};

/// Whether this build can reach the Secret Service (built with libsecret).
[[nodiscard]] bool secretServiceBuilt() noexcept;

class SecretStore {
  public:
    /// Files go in `dir`; `keyring` false keeps every secret there and never asks the keyring.
    SecretStore(std::filesystem::path dir, bool keyring);

    /// The secret named `name` and where it was, from the keyring first; nothing when none is
    /// kept. `error` is set only when a place could not be read.
    struct Found {
        std::string value;
        SecretPlace place{SecretPlace::File};
    };
    [[nodiscard]] std::optional<Found> read(std::string_view name, std::string& error) const;

    /// Keeps `value` under `name`, in the keyring when it takes it, else in the file; the other
    /// place is cleared. Nothing when neither took it; `error` says why.
    [[nodiscard]] std::optional<SecretPlace> save(std::string_view name, std::string_view label,
                                                  std::string_view value, std::string& error) const;

    /// Forgets `name` in both places. False with `error` when one could not be cleared.
    [[nodiscard]] bool forget(std::string_view name, std::string& error) const;

  private:
    [[nodiscard]] std::filesystem::path fileOf(std::string_view name) const;
    [[nodiscard]] bool usesKeyring() const noexcept;

    std::filesystem::path dir_;
    bool keyring_;
};

} // namespace opensu::host
