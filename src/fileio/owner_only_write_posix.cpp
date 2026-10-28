#include "owner_only_write.hpp"

#include <cerrno>
#include <cstring>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>

namespace opensu::fileio {
namespace {

namespace fs = std::filesystem;

/// Writes all of `bytes` to `fd`, then flushes it to disk.
bool writeAll(int fd, std::string_view bytes) {
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

bool failed(const fs::path& file, const char* what, int code, std::string& error) {
    error = std::string{"cannot "} + what + " " + file.string() + ": " + std::strerror(code);
    return false;
}

} // namespace

bool writeOwnerOnly(const fs::path& file, std::string_view bytes, std::string& error) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if (ec) {
        error = "cannot create " + file.parent_path().string() + ": " + ec.message();
        return false;
    }
    fs::permissions(file.parent_path(), fs::perms::owner_all, ec);

    // mkstemp creates the file owner-only, and a name of its own per writer.
    std::string staged = file.string() + ".XXXXXX";
    const int fd = ::mkostemp(staged.data(), O_CLOEXEC);
    if (fd < 0) {
        return failed(file, "create", errno, error);
    }
    const bool written = writeAll(fd, bytes);
    const int writeErrno = errno;
    ::close(fd);
    if (!written) {
        fs::remove(staged, ec);
        return failed(file, "write", writeErrno, error);
    }
    if (::rename(staged.c_str(), file.c_str()) != 0) {
        const int renameErrno = errno;
        fs::remove(staged, ec);
        return failed(file, "replace", renameErrno, error);
    }
    return true;
}

} // namespace opensu::fileio
