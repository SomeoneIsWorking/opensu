// The socket a Wine game's winemac.drv shows its windows through on macOS: hello is answered with
// the screen and the focus, windows are kept in z order and dropped when gone, input goes to the
// process whose window is on top, and the socket file goes with the launch.
#include "embed_socket.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "support/test_main.hpp"

namespace {

using opensu::app::EmbedSocket;
using Json = nlohmann::json;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

/// winemac's end: one connection, lines in and out.
class Peer {
  public:
    explicit Peer(const std::string& path) : fd_{::socket(AF_UNIX, SOCK_STREAM, 0)} {
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        path.copy(address.sun_path, sizeof address.sun_path - 1);
        expect(fd_ >= 0 &&
                   ::connect(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof address) == 0,
               "winemac connects");
    }
    ~Peer() {
        ::close(fd_);
    }
    Peer(const Peer&) = delete;
    Peer& operator=(const Peer&) = delete;

    void send(const Json& message) const {
        const std::string line = message.dump() + "\n";
        expect(::send(fd_, line.data(), line.size(), 0) == static_cast<ssize_t>(line.size()),
               "a line goes out");
    }
    /// The next line from openSU, polling `embed` while waiting.
    Json next(EmbedSocket& embed) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{1};
        while (std::chrono::steady_clock::now() < until) {
            if (const std::size_t end = buffer_.find('\n'); end != std::string::npos) {
                Json line = Json::parse(buffer_.substr(0, end));
                buffer_.erase(0, end + 1);
                return line;
            }
            embed.poll();
            char chunk[512];
            const ssize_t got = ::recv(fd_, chunk, sizeof chunk, MSG_DONTWAIT);
            if (got > 0) {
                buffer_.append(chunk, static_cast<std::size_t>(got));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds{2});
            }
        }
        expect(false, "openSU answered in time");
        return {};
    }

  private:
    int fd_;
    std::string buffer_;
};

/// Polls until `done` or a second has passed.
template <class Done> void pollUntil(EmbedSocket& embed, Done done) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{1};
    while (!done() && std::chrono::steady_clock::now() < until) {
        embed.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
}

Json window(int id, int z, double width) {
    return Json{{"window", id},   {"context", 1000 + id}, {"x", 0},     {"y", 0},
                {"width", width}, {"height", 100},        {"scale", 2}, {"title", "t"},
                {"z", z},         {"kind", "normal"}};
}

void talks() {
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / ("opensu-embed-" + std::to_string(::getpid()));
    EmbedSocket embed{directory, [] {
                          return std::string{"tree"};
                      }};
    embed.setScreen({.width = 1280, .height = 720, .scale = 2});
    const std::optional<std::string> variable = embed.open();
    const std::string named = variable.value_or("");
    expect(named.starts_with("OPENSU_EMBED_SOCKET="), "the game gets the variable");
    const std::string path = named.substr(std::string{"OPENSU_EMBED_SOCKET="}.size());

    Peer game{path};
    game.send({{"hello", 1}, {"pid", ::getpid()}});
    const Json screen = game.next(embed);
    expect(screen["screen"]["width"] == 1280 && screen["screen"]["height"] == 720,
           "hello is answered with the screen");
    expect(game.next(embed)["focus"] == true, "and the focus");
    expect(embed.gameSaidHello(), "a process other than explorer.exe embeds");
    expect(!embed.sawWindow(), "no window yet");

    game.send(window(7, 2, 300));
    game.send(window(8, 1, 200));
    pollUntil(embed, [&] {
        return embed.windows().size() == 2;
    });
    expect(embed.windows().size() == 2 && embed.windows()[0].id == 8 &&
               embed.windows()[1].id == 7 && embed.windows()[1].context == 1007,
           "windows are kept lowest first");
    expect(embed.sawWindow(), "a window was seen");

    embed.key({.down = true, .keycode = 0, .chars = "a", .modifiers = 0, .repeat = false});
    const Json key = game.next(embed);
    expect(key["key"]["keycode"] == 0 && key["key"]["chars"] == "a" && key["key"]["down"] == true,
           "a key reaches the process on top");
    embed.mouse(embed.windows()[1], 10, 20, 1);
    const Json mouse = game.next(embed);
    expect(mouse["mouse"]["window"] == 7 && mouse["mouse"]["buttons"] == 1,
           "a click names its window");
    embed.setFocus(false);
    expect(game.next(embed)["focus"] == false, "losing the input is told");
    embed.setScreen({.width = 1600, .height = 900, .scale = 2});
    expect(game.next(embed)["screen"]["width"] == 1600, "a new screen is told");

    game.send({{"window", 8}, {"gone", true}});
    game.send({{"cursor", "hidden"}});
    pollUntil(embed, [&] {
        return embed.windows().size() == 1 && embed.cursorHidden();
    });
    expect(embed.windows().size() == 1 && embed.cursorHidden(),
           "gone windows go, the cursor follows");

    embed.close();
    expect(!std::filesystem::exists(path), "the socket goes with the launch");
    expect(!embed.sawWindow() && embed.windows().empty(), "and what it reported");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() try {
    talks();
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
