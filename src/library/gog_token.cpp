#include "gog_token.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <unistd.h>

namespace iideck::library::gog {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

constexpr fs::perms ownerOnlyDir = fs::perms::owner_all;

[[noreturn]] void failWrite(const fs::path& file, const char* what) {
    throw std::runtime_error{std::string{"cannot save the GOG sign-in to "} + file.string() + ": " +
                             what + ": " + std::strerror(errno)};
}

/// Writes all of `bytes` to `fd`, then flushes it to disk.
bool writeAll(int fd, const std::string& bytes) {
    std::size_t done = 0;
    while (done < bytes.size()) {
        const ssize_t wrote = ::write(fd, bytes.data() + done, bytes.size() - done);
        if (wrote < 0 && errno == EINTR) {
            continue;
        }
        if (wrote <= 0) {
            return false;
        }
        done += static_cast<std::size_t>(wrote);
    }
    return ::fsync(fd) == 0;
}

} // namespace

TokenStore::TokenStore(fs::path file) : file_{std::move(file)} {
}

TokenStore TokenStore::under(const fs::path& dataDir) {
    return TokenStore{dataDir / "gog-token.json"};
}

std::optional<Token> TokenStore::load() const {
    std::error_code ec;
    if (!fs::exists(file_, ec)) {
        return std::nullopt;
    }
    std::ifstream in{file_};
    const json document = json::parse(in, nullptr, false);
    if (!document.is_object() || !document.contains("access_token") ||
        !document.contains("refresh_token") || !document.contains("expires_at")) {
        throw std::runtime_error{"the saved GOG sign-in is not a token"};
    }
    Token token;
    token.accessToken = document.value("access_token", "");
    token.refreshToken = document.value("refresh_token", "");
    token.userId = document.value("user_id", "");
    token.expiresAt = document.value("expires_at", std::int64_t{0});
    return token;
}

void TokenStore::save(const Token& token) const {
    std::error_code ec;
    fs::create_directories(file_.parent_path(), ec);
    if (ec) {
        throw std::runtime_error{"cannot create " + file_.parent_path().string() + ": " +
                                 ec.message()};
    }
    fs::permissions(file_.parent_path(), ownerOnlyDir, ec);

    const json document{{"access_token", token.accessToken},
                        {"refresh_token", token.refreshToken},
                        {"user_id", token.userId},
                        {"expires_at", token.expiresAt}};
    // mkstemp creates the file owner-only, and a name of its own per writer.
    std::string staged = file_.string() + ".XXXXXX";
    const int fd = ::mkostemp(staged.data(), O_CLOEXEC);
    if (fd < 0) {
        failWrite(file_, "create");
    }
    const bool written = writeAll(fd, document.dump());
    const int writeErrno = errno;
    ::close(fd);
    if (!written) {
        errno = writeErrno;
        fs::remove(staged, ec);
        failWrite(file_, "write");
    }
    if (::rename(staged.c_str(), file_.c_str()) != 0) {
        const int renameErrno = errno;
        fs::remove(staged, ec);
        errno = renameErrno;
        failWrite(file_, "rename");
    }
}

} // namespace iideck::library::gog
