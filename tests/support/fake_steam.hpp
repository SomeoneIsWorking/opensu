// A fake Steam client: serves the DevTools page list and answers Runtime.evaluate in the
// SharedJSContext with one scripted answer per expression, recording each in order. Browser views
// are pages a test adds (`putPage`), each with its own scripted answers; `/json/close/<id>` closes
// one. Every connection is served on its own thread, as DevTools serves several at once.
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
#include <map>
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
    /// The answer a browser view gives: its id and the expression.
    using PageAnswer = std::function<json(const std::string& page, const std::string& expression)>;

    /// One expression a browser view was asked, with the URL it showed then.
    struct PageCall {
        std::string page;
        std::string url;
        std::string expression;
    };

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
        {
            const std::lock_guard lock{mutex_};
            for (const int client : clients_) {
                ::shutdown(client, SHUT_RDWR);
            }
        }
        for (std::thread& connection : connections_) {
            connection.join();
        }
        for (const int client : clients_) {
            ::close(client);
        }
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

    /// The browser views' answers.
    void answerPages(PageAnswer answer) {
        const std::lock_guard lock{mutex_};
        pageAnswer_ = std::move(answer);
    }
    /// Lists a browser view `id` showing `url`, or moves it there.
    void putPage(const std::string& id, const std::string& url) {
        const std::lock_guard lock{mutex_};
        pages_[id] = url;
    }
    /// The URL view `id` shows; empty once it is closed.
    [[nodiscard]] std::string pageUrl(const std::string& id) const {
        const std::lock_guard lock{mutex_};
        const auto found = pages_.find(id);
        return found == pages_.end() ? std::string{} : found->second;
    }
    /// Takes view `id` off the list, as the player closing its window does.
    void removePage(const std::string& id) {
        const std::lock_guard lock{mutex_};
        pages_.erase(id);
    }
    [[nodiscard]] std::vector<PageCall> pageCalls() const {
        const std::lock_guard lock{mutex_};
        return pageCalls_;
    }
    /// The views closed through `/json/close`.
    [[nodiscard]] std::vector<std::string> closed() const {
        const std::lock_guard lock{mutex_};
        return closed_;
    }
    /// How many times the page list was read.
    [[nodiscard]] int listings() const {
        const std::lock_guard lock{mutex_};
        return listings_;
    }

  private:
    void serve() {
        while (!stopping_) {
            const int client = ::accept(listener_, nullptr, nullptr);
            if (client < 0) {
                return;
            }
            const std::lock_guard lock{mutex_};
            clients_.push_back(client);
            connections_.emplace_back([this, client] {
                handle(client);
                ::shutdown(client, SHUT_RDWR);
            });
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
            json pages = json::array({{{"id", "MAIN"},
                                       {"type", "page"},
                                       {"title", "Steam"},
                                       {"url", "https://steamloopback.host/routes/library"},
                                       {"webSocketDebuggerUrl", url("/devtools/page/MAIN")}},
                                      {{"id", "SHARED"},
                                       {"type", "page"},
                                       {"title", "SharedJSContext"},
                                       {"url", "https://steamloopback.host/index.html"},
                                       {"webSocketDebuggerUrl", url("/devtools/page/SHARED")}}});
            {
                const std::lock_guard lock{mutex_};
                ++listings_;
                for (const auto& [id, at] : pages_) {
                    pages.push_back({{"id", id},
                                     {"type", "page"},
                                     {"title", at},
                                     {"url", at},
                                     {"webSocketDebuggerUrl", url("/devtools/page/" + id)}});
                }
            }
            sendBody(client, 200, pages.dump());
            return;
        }
        const std::string closePrefix = "GET /json/close/";
        if (head.starts_with(closePrefix)) {
            const std::size_t end = head.find(' ', closePrefix.size());
            const std::string id = head.substr(closePrefix.size(), end - closePrefix.size());
            bool known = false;
            {
                const std::lock_guard lock{mutex_};
                known = pages_.erase(id) > 0;
                closed_.push_back(id);
            }
            sendBody(client, known ? 200 : 404, known ? "Target is closing" : "No such target");
            return;
        }
        const std::string pagePrefix = "GET /devtools/page/";
        fakeSteamCheck(head.starts_with(pagePrefix), "a page is opened");
        const std::string page =
            head.substr(pagePrefix.size(), head.find(' ', pagePrefix.size()) - pagePrefix.size());
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
            json value;
            if (page == "SHARED") {
                {
                    const std::lock_guard lock{mutex_};
                    calls_.push_back(expression);
                }
                value = answer_(expression);
            } else {
                PageAnswer answer;
                {
                    const std::lock_guard lock{mutex_};
                    const auto shown = pages_.find(page);
                    pageCalls_.push_back(
                        {page, shown == pages_.end() ? std::string{} : shown->second, expression});
                    answer = pageAnswer_;
                }
                value = answer ? answer(page, expression) : json{};
            }
            // An unrelated event first, as DevTools sends them on the same socket.
            sendFrame(client, json{{"method", "Runtime.consoleAPICalled"}}.dump());
            const json reply{{"id", request["id"]},
                             {"result", {{"result", {{"type", "object"}, {"value", value}}}}}};
            sendFrame(client, reply.dump());
        }
    }

    static void sendBody(int client, int status, const std::string& body) {
        sendAll(client, "HTTP/1.1 " + std::to_string(status) +
                            " Fake\r\nContent-Length:" + std::to_string(body.size()) +
                            "\r\nContent-Type:application/json\r\n\r\n" + body);
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
    PageAnswer pageAnswer_;
    std::map<std::string, std::string> pages_;
    std::vector<PageCall> pageCalls_;
    std::vector<std::string> closed_;
    int listings_{0};
    std::vector<int> clients_;
    std::vector<std::thread> connections_;
    std::thread thread_;
};

} // namespace opensu::test
