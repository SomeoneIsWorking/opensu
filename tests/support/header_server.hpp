// A loopback HTTP server that shows its handler the request line and headers, which
// lucent::http::Server does not keep.
#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace iideck::test {

struct Answer {
    int status{200};
    std::string body;
};

class HeaderServer {
  public:
    /// Gets the request target (path and query) and the raw header block.
    using Handler = std::function<Answer(const std::string& target, const std::string& headers)>;

    explicit HeaderServer(Handler handler) : handler_{std::move(handler)} {
        listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t length = sizeof(address);
        if (listener_ < 0 ||
            ::bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
            ::listen(listener_, 8) != 0 ||
            ::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
            std::fprintf(stderr, "FAIL: the header server cannot listen\n");
            std::exit(1);
        }
        port_ = ntohs(address.sin_port);
        worker_ = std::thread{[this] {
            serve();
        }};
    }

    ~HeaderServer() {
        stop_ = true;
        worker_.join();
        ::close(listener_);
    }
    HeaderServer(const HeaderServer&) = delete;
    HeaderServer& operator=(const HeaderServer&) = delete;

    [[nodiscard]] std::string base() const {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

  private:
    void serve() {
        while (!stop_) {
            pollfd waiting{listener_, POLLIN, 0};
            if (::poll(&waiting, 1, 20) <= 0) {
                continue;
            }
            const int client = ::accept(listener_, nullptr, nullptr);
            if (client >= 0) {
                answer(client);
                ::close(client);
            }
        }
    }

    void answer(int client) const {
        std::string request;
        char buffer[2048];
        while (request.find("\r\n\r\n") == std::string::npos) {
            const ssize_t got = ::recv(client, buffer, sizeof(buffer), 0);
            if (got <= 0) {
                return;
            }
            request.append(buffer, static_cast<std::size_t>(got));
        }
        const std::size_t targetStart = request.find(' ') + 1;
        const std::size_t targetEnd = request.find(' ', targetStart);
        const Answer reply = handler_(request.substr(targetStart, targetEnd - targetStart),
                                      request.substr(request.find("\r\n") + 2));
        const std::string bytes = "HTTP/1.1 " + std::to_string(reply.status) +
                                  " X\r\nContent-Length: " + std::to_string(reply.body.size()) +
                                  "\r\nConnection: close\r\n\r\n" + reply.body;
        std::size_t sent = 0;
        while (sent < bytes.size()) {
            const ssize_t put =
                ::send(client, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
            if (put <= 0) {
                return;
            }
            sent += static_cast<std::size_t>(put);
        }
    }

    Handler handler_;
    int listener_{-1};
    std::uint16_t port_{0};
    std::atomic<bool> stop_{false};
    std::thread worker_;
};

} // namespace iideck::test
