// trust_store — the certificates HTTPS is checked against: libcurl's own default on the desktop;
// on Android, whose libcurl has none compiled in, the system's CA files read once into memory
// (their names use OpenSSL's old subject hash, which a CA directory lookup no longer matches).
#pragma once

#include <string>

namespace opensu::net {

class TrustStore {
  public:
    TrustStore();

    /// The trusted certificates as concatenated PEM; empty to keep libcurl's default.
    [[nodiscard]] const std::string& pem() const noexcept {
        return pem_;
    }

  private:
    std::string pem_;
};

} // namespace opensu::net
