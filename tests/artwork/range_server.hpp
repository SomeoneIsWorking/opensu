// A loopback HTTP server that answers GET with Range, which lucent::http::Server cannot: its
// requests carry no headers.
#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace iideck::artwork::fixture {

class RangeServer {
  public:
    RangeServer() {
        listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t length = sizeof(address);
        if (listener_ < 0 ||
            ::bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
            ::listen(listener_, 8) != 0 ||
            ::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
            std::fprintf(stderr, "FAIL: the range server cannot listen\n");
            std::exit(1);
        }
        port_ = ntohs(address.sin_port);
        worker_ = std::thread{[this] {
            serve();
        }};
    }

    ~RangeServer() {
        stop_ = true;
        worker_.join();
        ::close(listener_);
    }
    RangeServer(const RangeServer&) = delete;
    RangeServer& operator=(const RangeServer&) = delete;

    [[nodiscard]] std::string base() const {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

    /// Serves `body` at `path`.
    void put(const std::string& path, std::string body) {
        const std::lock_guard lock{mutex_};
        files_[path] = std::move(body);
    }
    /// Answers every request 200 with the whole body, as a server that ignores Range.
    void ignoreRange(bool ignore) {
        ignoreRange_ = ignore;
    }
    [[nodiscard]] std::uint64_t requests() const {
        return requests_;
    }
    [[nodiscard]] std::uint64_t bytesSent() const {
        return bytesSent_;
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

    void answer(int client) {
        std::string request;
        char buffer[2048];
        while (request.find("\r\n\r\n") == std::string::npos) {
            const ssize_t got = ::recv(client, buffer, sizeof(buffer), 0);
            if (got <= 0) {
                return;
            }
            request.append(buffer, static_cast<std::size_t>(got));
        }
        ++requests_;
        const std::size_t pathStart = request.find(' ') + 1;
        const std::string path =
            request.substr(pathStart, request.find(' ', pathStart) - pathStart);
        std::string body;
        {
            const std::lock_guard lock{mutex_};
            const auto file = files_.find(path);
            if (file == files_.end()) {
                send(client,
                     "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                return;
            }
            body = file->second;
        }
        const std::size_t rangeAt = request.find("Range: bytes=");
        if (rangeAt == std::string::npos || ignoreRange_) {
            send(client, "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) +
                             "\r\nConnection: close\r\n\r\n" + body);
            return;
        }
        const std::uint64_t first = std::strtoull(request.c_str() + rangeAt + 13, nullptr, 10);
        const std::size_t dash = request.find('-', rangeAt);
        std::uint64_t last = std::strtoull(request.c_str() + dash + 1, nullptr, 10);
        last = std::min<std::uint64_t>(last, body.size() - 1);
        const std::string part = body.substr(first, last - first + 1);
        send(client, "HTTP/1.1 206 Partial Content\r\nContent-Range: bytes " +
                         std::to_string(first) + "-" + std::to_string(last) + "/" +
                         std::to_string(body.size()) + "\r\nContent-Length: " +
                         std::to_string(part.size()) + "\r\nConnection: close\r\n\r\n" + part);
    }

    void send(int client, const std::string& bytes) {
        std::size_t sent = 0;
        while (sent < bytes.size()) {
            const ssize_t put =
                ::send(client, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
            if (put <= 0) {
                return;
            }
            sent += static_cast<std::size_t>(put);
        }
        bytesSent_ += bytes.size();
    }

    int listener_{-1};
    std::uint16_t port_{0};
    std::atomic<bool> stop_{false};
    std::atomic<bool> ignoreRange_{false};
    std::atomic<std::uint64_t> requests_{0};
    std::atomic<std::uint64_t> bytesSent_{0};
    std::mutex mutex_;
    std::map<std::string, std::string> files_;
    std::thread worker_;
};

} // namespace iideck::artwork::fixture
