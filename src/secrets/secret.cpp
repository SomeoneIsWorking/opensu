#include "secret.hpp"

#include <openssl/crypto.h>

namespace opensu::secrets {

void wipeBytes(void* data, std::size_t size) {
    OPENSSL_cleanse(data, size);
}

} // namespace opensu::secrets
