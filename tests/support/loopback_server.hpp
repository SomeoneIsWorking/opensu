// A started lucent::http::Server on a free loopback port, standing in for a remote service.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#include "lucent/http.h"

namespace opensu::test {

class LoopbackServer {
  public:
    explicit LoopbackServer(lucent::http::Handler handler) : server_{{}, std::move(handler)} {
        if (!server_.start()) {
            std::fprintf(stderr, "FAIL: the loopback server cannot listen\n");
            std::exit(1);
        }
    }

    [[nodiscard]] std::string base() const {
        return "http://127.0.0.1:" + std::to_string(server_.port());
    }

  private:
    lucent::http::Server server_;
};

/// A reply carrying `body` with `status`.
inline lucent::http::Response reply(int status, std::string body) {
    return lucent::http::Response::json(status, "Test", std::move(body));
}

} // namespace opensu::test
