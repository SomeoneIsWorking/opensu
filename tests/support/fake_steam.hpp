// A fake Steam client: serves the DevTools page list and answers Runtime.evaluate in the
// SharedJSContext with one scripted answer per expression, recording each in order.
#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "lucent/http_client.h"

namespace opensu::test {

inline void fakeSteamCheck(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

/// A fake Steam client: one script answer per expression, recorded in order.
class FakeSteam {
  public:
    using json = nlohmann::json;
    using Answer = std::function<json(const std::string& expression)>;

    explicit FakeSteam(Answer answer) : answer_{std::move(answer)} {
        listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t length = sizeof(address);
        const bool listening =
            ::bind(listener_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 &&
            ::listen(listener_, 4) == 0 &&
            ::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length) == 0;
        fakeSteamCheck(listening, "the fake Steam listens");
        port_ = ntohs(address.sin_port);
        thread_ = std::thread{[this] {
            serve();
        }};
    }
    ~FakeSteam() {
        stopping_ = true;
        ::shutdown(listener_, SHUT_RDWR);
        ::close(listener_);
        thread_.join();
    }
    FakeSteam(const FakeSteam&) = delete;
    FakeSteam& operator=(const FakeSteam&) = delete;
    FakeSteam(FakeSteam&&) = delete;
    FakeSteam& operator=(FakeSteam&&) = delete;

    [[nodiscard]] std::uint16_t port() const {
        return port_;
    }
    [[nodiscard]] std::vector<std::string> calls() const {
        const std::lock_guard lock{mutex_};
        return calls_;
    }

  private:
    void serve() {
        while (!stopping_) {
            const int client = ::accept(listener_, nullptr, nullptr);
            if (client < 0) {
                return;
            }
            handle(client);
            ::close(client);
        }
    }

    static bool readSome(int client, std::string& buffer) {
        std::array<char, 4096> chunk{};
        const ssize_t got = ::recv(client, chunk.data(), chunk.size(), 0);
        if (got <= 0) {
            return false;
        }
        buffer.append(chunk.data(), static_cast<std::size_t>(got));
        return true;
    }

    static void sendAll(int client, const std::string& bytes) {
        fakeSteamCheck(::send(client, bytes.data(), bytes.size(), MSG_NOSIGNAL) ==
                           static_cast<ssize_t>(bytes.size()),
                       "the fake Steam sends");
    }

    void handle(int client) {
        std::string buffer;
        while (buffer.find("\r\n\r\n") == std::string::npos) {
            if (!readSome(client, buffer)) {
                return;
            }
        }
        const std::string head = buffer.substr(0, buffer.find("\r\n\r\n"));
        buffer.erase(0, head.size() + 4);
        if (head.starts_with("GET /json ")) {
            const json pages = json::array(
                {{{"title", "Steam"}, {"webSocketDebuggerUrl", url("/devtools/page/MAIN")}},
                 {{"title", "SharedJSContext"},
                  {"webSocketDebuggerUrl", url("/devtools/page/SHARED")}}});
            const std::string body = pages.dump();
            sendAll(client, "HTTP/1.1 200 OK\r\nContent-Length:" + std::to_string(body.size()) +
                                "\r\nContent-Type:application/json\r\n\r\n" + body);
            return;
        }
        fakeSteamCheck(head.starts_with("GET /devtools/page/SHARED "),
                       "the shared context is opened");
        const std::string marker = "Sec-WebSocket-Key: ";
        const std::size_t at = head.find(marker) + marker.size();
        const std::string key = head.substr(at, head.find("\r\n", at) - at);
        sendAll(client, "HTTP/1.1 101 WebSocket Protocol Handshake\r\nUpgrade: WebSocket\r\n"
                        "Connection: Upgrade\r\nSec-WebSocket-Accept: " +
                            lucent::http::websocket_accept(key) + "\r\n\r\n");
        for (;;) {
            std::optional<std::string> message = readFrame(client, buffer);
            if (!message) {
                return;
            }
            const json request = json::parse(*message);
            const std::string expression = request["params"]["expression"];
            {
                const std::lock_guard lock{mutex_};
                calls_.push_back(expression);
            }
            // An unrelated event first, as DevTools sends them on the same socket.
            sendFrame(client, json{{"method", "Runtime.consoleAPICalled"}}.dump());
            const json reply{
                {"id", request["id"]},
                {"result", {{"result", {{"type", "object"}, {"value", answer_(expression)}}}}}};
            sendFrame(client, reply.dump());
        }
    }

    static std::optional<std::string> readFrame(int client, std::string& buffer) {
        const auto need = [&](std::size_t size) {
            while (buffer.size() < size) {
                if (!readSome(client, buffer)) {
                    return false;
                }
            }
            return true;
        };
        if (!need(2)) {
            return std::nullopt;
        }
        const auto opcode = static_cast<std::uint8_t>(buffer[0]) & 0x0fu;
        std::size_t size = static_cast<std::uint8_t>(buffer[1]) & 0x7fu;
        std::size_t offset = 2;
        if (size == 126) {
            if (!need(4)) {
                return std::nullopt;
            }
            size = (static_cast<std::size_t>(static_cast<std::uint8_t>(buffer[2])) << 8) |
                   static_cast<std::uint8_t>(buffer[3]);
            offset = 4;
        }
        if (!need(offset + 4 + size)) {
            return std::nullopt;
        }
        std::string payload = buffer.substr(offset + 4, size);
        for (std::size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<char>(payload[i] ^ buffer[offset + i % 4]);
        }
        buffer.erase(0, offset + 4 + size);
        if (opcode == 0x8) {
            return std::nullopt;
        }
        return payload;
    }

    static void sendFrame(int client, const std::string& payload) {
        std::string frame{static_cast<char>(0x81)};
        if (payload.size() < 126) {
            frame.push_back(static_cast<char>(payload.size()));
        } else {
            frame.push_back(static_cast<char>(126));
            frame.push_back(static_cast<char>((payload.size() >> 8) & 0xffu));
            frame.push_back(static_cast<char>(payload.size() & 0xffu));
        }
        sendAll(client, frame + payload);
    }

    [[nodiscard]] std::string url(const std::string& path) const {
        return "ws://127.0.0.1:" + std::to_string(port_) + path;
    }

    Answer answer_;
    int listener_{-1};
    std::uint16_t port_{0};
    std::atomic<bool> stopping_{false};
    mutable std::mutex mutex_;
    std::vector<std::string> calls_;
    std::thread thread_;
};

} // namespace opensu::test
