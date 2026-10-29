// A test program's last handler: an exception its checks let out fails the test with its message.
#pragma once

#include <cstdio>
#include <exception>

namespace opensu::test {

/// For `int main() try { ... } catch (...) { return opensu::test::escaped(); }`: names the
/// exception on stderr and gives main's failing exit code.
inline int escaped() noexcept {
    try {
        throw;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: uncaught exception: %s\n", error.what());
    } catch (...) {
        std::fprintf(stderr, "FAIL: uncaught exception\n");
    }
    return 1;
}

} // namespace opensu::test
