#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "lucent/log.h"
#include "trust_store.hpp"

namespace opensu::net {

namespace {

namespace fs = std::filesystem;

/// The updatable store since Android 14, then the one in the system image.
constexpr std::array<const char*, 2> caFolders{"/apex/com.android.conscrypt/cacerts",
                                               "/system/etc/security/cacerts"};

} // namespace

TrustStore::TrustStore() {
    for (const char* folder : caFolders) {
        std::error_code ec;
        for (const fs::directory_entry& entry : fs::directory_iterator{folder, ec}) {
            std::ifstream file{entry.path(), std::ios::binary};
            pem_.append(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
            pem_.push_back('\n');
        }
        if (!pem_.empty()) {
            return;
        }
    }
    lucent::warn("net", "no system CA certificates found; HTTPS requests will fail");
}

} // namespace opensu::net
