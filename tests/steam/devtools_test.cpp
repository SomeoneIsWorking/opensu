// The DevTools link, the download queue and the installer, against a fake Steam that serves
// the DevTools page list and answers Runtime.evaluate like the SharedJSContext.
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "devtools.hpp"
#include "downloads.hpp"
#include "install_wizard.hpp"
#include "lucent/http_client.h"

namespace {

using iideck::steam::DevTools;
using iideck::steam::Download;
using iideck::steam::DownloadQueue;
using iideck::steam::InstallStep;
using iideck::steam::InstallWizard;
using nlohmann::json;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

/// A fake Steam client: one script answer per expression, recorded in order.
class FakeSteam {
  public:
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
        expect(listening, "the fake Steam listens");
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
        expect(::send(client, bytes.data(), bytes.size(), MSG_NOSIGNAL) ==
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
        expect(head.starts_with("GET /devtools/page/SHARED "), "the shared context is opened");
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

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

void testDownloadsParse() {
    const json answer = json::parse(R"({
      "overview": {"update_appid": 960090, "paused": false, "overall_percent_complete": 53,
                   "overall_estimated_time_remaining_sec": 34},
      "items": [{"remote_client_id": "0", "item_data": [
        {"appid": 960090, "active": false, "paused": false, "completed": false,
         "update_type_info": [{"has_update": true, "completed_update": false,
                               "overall_percent_complete": 40}]},
        {"appid": 292030, "active": false, "paused": true, "completed": false,
         "update_type_info": [{"has_update": false, "completed_update": false,
                               "overall_percent_complete": 0},
                              {"has_update": true, "completed_update": false,
                               "overall_percent_complete": 10}]},
        {"appid": 480, "active": false, "paused": false, "completed": true,
         "update_type_info": [{"has_update": true, "completed_update": true,
                               "overall_percent_complete": 100}]}]},
        {"remote_client_id": "77", "item_data": [
        {"appid": 1, "completed": false,
         "update_type_info": [{"has_update": true, "overall_percent_complete": 5}]}]}]
    })");
    const std::vector<Download> downloads = iideck::steam::parseDownloads(answer);
    expect(downloads.size() == 2, "finished and remote downloads are left out");
    expect(downloads[0].appId == "960090" && downloads[0].active && downloads[0].progress == 0.53 &&
               downloads[0].secondsLeft == 34,
           "the active download takes the overview's live figures");
    expect(downloads[1].appId == "292030" && !downloads[1].active && downloads[1].paused &&
               downloads[1].progress == 0.10 && !downloads[1].secondsLeft,
           "a queued download keeps its own part's progress");
    expect(iideck::steam::parseDownloads(json::object()).empty(), "no answer, no downloads");
}

void testQueueReadsThroughDevTools() {
    FakeSteam steam{[](const std::string& expression) {
        expect(contains(expression, "RegisterForDownloadItems"), "the queue asks for items");
        return json::parse(R"({"overview": {"update_appid": 0},
            "items": [{"remote_client_id": "0", "item_data": [{"appid": 7, "completed": false,
            "update_type_info": [{"has_update": true, "overall_percent_complete": 25}]}]}]})");
    }};
    DevTools devTools{steam.port()};
    DownloadQueue queue{devTools};
    std::string error;
    const std::optional<std::vector<Download>> first = queue.read(error);
    expect(first && first->size() == 1 && (*first)[0].appId == "7" && (*first)[0].progress == 0.25,
           "the queue is read from Steam");
    expect(queue.read(error).has_value() && steam.calls().size() == 2,
           "a second read reuses the connection");
}

void testUnreachableSteam() {
    std::uint16_t port = 0;
    {
        const FakeSteam gone{[](const std::string&) {
            return json{};
        }};
        port = gone.port();
    }
    DevTools devTools{port};
    std::string error;
    expect(!devTools.evaluate("1", error) && !error.empty(), "a closed port is reported");
}

void testInstallWalksTheWizard() {
    // Opened, then app info, then config, then a licence, then handed off.
    std::vector<int> states{5, 7, 8, 0};
    std::size_t at = 0;
    FakeSteam steam{[&states, &at](const std::string& expression) -> json {
        if (contains(expression, "GetInstallManagerInfo")) {
            const int state = states[std::min(at, states.size() - 1)];
            return {{"state", state}, {"app", state == 0 ? 0 : 480}, {"error", ""}};
        }
        if (contains(expression, "ContinueInstall")) {
            ++at;
            return json{};
        }
        if (contains(expression, "LoadEula")) {
            return json::array({{{"id", "480_eula_0"}, {"url", "http://eula"}, {"version", 2}}});
        }
        if (contains(expression, "OpenInstallWizard")) {
            return json{};
        }
        if (contains(expression, "MarkEulaAccepted")) {
            return json{};
        }
        expect(false, "only installer calls are made");
        return json{};
    }};
    DevTools devTools{steam.port()};
    InstallWizard wizard{devTools};

    expect(wizard.open("480").kind == InstallStep::Kind::Working, "the wizard waits on app info");
    at = 1;
    expect(wizard.poll().kind == InstallStep::Kind::Working, "the config step is continued");
    const InstallStep eula = wizard.poll();
    expect(eula.kind == InstallStep::Kind::NeedsEula && eula.eulas.size() == 1 &&
               eula.eulas[0].id == "480_eula_0" && eula.eulas[0].version == 2,
           "a licence is handed to the player, not accepted");
    expect(wizard.accept(eula.eulas).kind == InstallStep::Kind::Working,
           "an accepted licence continues the install");
    expect(wizard.poll().kind == InstallStep::Kind::Queued, "the handed-off install is queued");

    const std::vector<std::string> calls = steam.calls();
    bool marked = false;
    for (const std::string& call : calls) {
        marked = marked || call == "SteamClient.Apps.MarkEulaAccepted(480,\"480_eula_0\",2)";
    }
    expect(marked, "the accepted licence is recorded with its id and version");
}

void testInstallRefusesWhatOnlySteamCanAsk() {
    FakeSteam steam{[](const std::string& expression) -> json {
        if (contains(expression, "GetInstallManagerInfo")) {
            return {{"state", 4}, {"app", 480}, {"error", ""}};
        }
        return json{};
    }};
    DevTools devTools{steam.port()};
    InstallWizard wizard{devTools};
    const InstallStep step = wizard.open("480");
    expect(step.kind == InstallStep::Kind::Failed && !step.failure.empty(),
           "a CD key prompt fails the install");
    expect(contains(steam.calls().back(), "CancelInstall"), "and closes Steam's wizard");
    expect(wizard.open("48;0").kind == InstallStep::Kind::Failed,
           "an app id that is not a number never reaches Steam");
}

} // namespace

int main() {
    testDownloadsParse();
    testQueueReadsThroughDevTools();
    testUnreachableSteam();
    testInstallWalksTheWizard();
    testInstallRefusesWhatOnlySteamCanAsk();
    std::printf("devtools: all checks passed\n");
    return 0;
}
