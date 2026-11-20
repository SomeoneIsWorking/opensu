// embed_socket — openSU's end of the socket a Wine game's winemac.drv shows its windows through on
// macOS (docs/plans/mac-wine-embed.md): one listening socket per launch, made in the runtime
// directory before the game starts and named to it in `OPENSU_EMBED_SOCKET`, read on the shell's
// loop. It keeps the windows winemac reports and the processes that said hello, answers each hello
// with the screen and the focus, and sends the keys, mouse and wheel it is given to the game. What
// the windows look like and when the game has the input belong to the overlay that hosts them
// (`game_overlay_layer_host.mm`); the on-screen keyboard types through it
// (`game_typing_layer_host.cpp`). JSON lines on Lucent's `LineSocketServer`.
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lucent::net {
class LineSocketServer;
}

namespace opensu::app {

class EmbedSocket {
  public:
    /// A top-level window of the game, in Wine's points with the origin top left.
    struct Window {
        std::uint64_t id{};
        /// The connection that reported it, which its input goes to.
        std::uint32_t peer{};
        /// The `CAContext` its picture is in.
        std::uint32_t context{};
        double x{};
        double y{};
        double width{};
        double height{};
        double scale{1};
        std::string title;
        int z{};
        /// "normal", "dialog" or "popup".
        std::string kind;
    };
    /// A Wine process that connected.
    struct Hello {
        std::uint32_t peer{};
        int pid{};
        /// Its program's file name, as its command line names it.
        std::string program;
    };
    /// The display openSU gives the game, in points.
    struct Screen {
        double width{};
        double height{};
        double scale{1};
        bool operator==(const Screen&) const = default;
    };
    /// A key as the Mac keyboard pressed it.
    struct Key {
        bool down{};
        /// The Mac's virtual key code.
        int keycode{};
        std::string chars;
        /// NSEventModifierFlags.
        std::uint64_t modifiers{};
        bool repeat{};
    };

    /// The sockets go in `directory`. `launchTree` describes the running launch's processes for
    /// the log when a game is not shown embedded.
    EmbedSocket(std::filesystem::path directory, std::function<std::string()> launchTree);
    ~EmbedSocket();
    EmbedSocket(const EmbedSocket&) = delete;
    EmbedSocket& operator=(const EmbedSocket&) = delete;
    EmbedSocket(EmbedSocket&&) = delete;
    EmbedSocket& operator=(EmbedSocket&&) = delete;

    /// Listens for the next game, forgetting the last one's; the "NAME=value" its environment
    /// gets, or nothing when the socket cannot be made (logged).
    [[nodiscard]] std::optional<std::string> open();
    /// The game is over: the socket and what it reported go.
    void close();
    [[nodiscard]] bool isOpen() const noexcept {
        return server_ != nullptr;
    }

    /// Reads what the game sent; true when its windows changed. Never blocks.
    bool poll();
    /// The windows shown now, lowest first.
    [[nodiscard]] const std::vector<Window>& windows() const noexcept {
        return windows_;
    }
    [[nodiscard]] const std::vector<Hello>& hellos() const noexcept {
        return hellos_;
    }
    /// Whether a process other than Wine's own explorer.exe said hello: the game embeds.
    [[nodiscard]] bool gameSaidHello() const;
    /// Whether a window was ever reported this launch. Any thread.
    [[nodiscard]] bool sawWindow() const noexcept {
        return sawWindow_.load();
    }
    /// Whether the game hides its cursor.
    [[nodiscard]] bool cursorHidden() const noexcept {
        return cursorHidden_;
    }
    /// The launch's processes, for the log.
    [[nodiscard]] std::string launchTree() const {
        return launchTree_ ? launchTree_() : std::string{};
    }

    /// The display the game sees; sent to every process now and to each that says hello later.
    void setScreen(Screen screen);
    [[nodiscard]] const Screen& screen() const noexcept {
        return screen_;
    }
    /// Whether the game is the foreground app; sent like the screen.
    void setFocus(bool focused);

    /// A key, to the process whose window is on top.
    void key(const Key& key);
    /// The pointer over `window` at (`x`, `y`) in its points, with `buttons` held (left 1, right
    /// 2, middle 4).
    void mouse(const Window& window, double x, double y, unsigned buttons);
    void scroll(const Window& window, double dx, double dy, bool precise);
    /// Mouse motion while the game has hidden its cursor.
    void relative(double dx, double dy);

  private:
    void handle(std::uint32_t peer, std::string_view line);
    void sendTo(std::uint32_t peer, const std::string& line);
    /// The process whose window is on top, or the game's first process when none shows.
    [[nodiscard]] std::optional<std::uint32_t> front() const;

    std::filesystem::path directory_;
    std::function<std::string()> launchTree_;
    std::unique_ptr<lucent::net::LineSocketServer> server_;
    unsigned launches_{0};
    std::vector<Window> windows_;
    std::vector<Hello> hellos_;
    std::atomic<bool> sawWindow_{false};
    bool cursorHidden_{false};
    Screen screen_;
    bool focused_{true};
};

} // namespace opensu::app
