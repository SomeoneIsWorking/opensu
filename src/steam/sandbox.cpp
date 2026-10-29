#include "sandbox.hpp"

#include <algorithm>
#include <charconv>

namespace opensu::steam {
namespace {

namespace fs = std::filesystem;

/// Variables naming the desktop's session, which the sandbox does not have.
constexpr std::string_view desktopVariables[] = {"WAYLAND_DISPLAY", "XAUTHORITY",
                                                 "SESSION_MANAGER"};

void add(std::vector<std::string>& args, std::initializer_list<std::string_view> items) {
    for (const std::string_view item : items) {
        args.emplace_back(item);
    }
}

} // namespace

std::optional<int> x11DisplayNumber(std::string_view display) noexcept {
    if (!display.starts_with(':')) {
        return std::nullopt;
    }
    display.remove_prefix(1);
    std::string_view number = display;
    if (const std::size_t screen = number.find('.'); screen != std::string_view::npos) {
        number.remove_suffix(number.size() - screen);
    }
    int value = -1;
    const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), value);
    if (error != std::errc{} || end != number.data() + number.size() || value < 0) {
        return std::nullopt;
    }
    return value;
}

std::vector<fs::path> gpuNodes(const fs::path& dev) {
    std::vector<fs::path> nodes;
    std::error_code error;
    if (fs::is_directory(dev / "dri", error)) {
        nodes.push_back(dev / "dri");
    }
    for (const fs::directory_entry& entry : fs::directory_iterator{dev, error}) {
        if (entry.path().filename().string().starts_with("nvidia")) {
            nodes.push_back(entry.path());
        }
    }
    std::sort(nodes.begin(), nodes.end());
    return nodes;
}

std::vector<std::string> sandboxArguments(const SandboxPlaces& places,
                                          const SandboxDevices& devices, const fs::path& secretBus,
                                          std::string& failure) {
    const std::optional<int> number = x11DisplayNumber(places.display);
    if (!number) {
        failure =
            "Isolated Steam needs openSU's Gamescope display; DISPLAY is '" + places.display + "'";
        return {};
    }
    const std::string x11 = "/tmp/.X11-unix/X" + std::to_string(*number);
    const std::string runtime = places.runtimeDir.string();

    std::vector<std::string> args;
    add(args, {"--bind", "/", "/", "--dev", "/dev", "--tmpfs", "/dev/shm"});
    for (const fs::path& node : places.gpu) {
        add(args, {"--dev-bind", node.string(), node.string()});
    }
    if (devices.allInput) {
        add(args, {"--dev-bind", "/dev/input", "/dev/input"});
    } else {
        add(args, {"--dir", "/dev/input"});
        for (const fs::path& node : devices.inputNodes) {
            add(args, {"--dev-bind", node.string(), node.string()});
        }
        // SDL and Steam would list every controller from udev's database.
        add(args, {"--tmpfs", "/run/udev"});
    }
    add(args, {"--tmpfs", "/tmp", "--dir", "/tmp/.X11-unix", "--bind", x11, x11});
    add(args, {"--tmpfs", runtime, "--chmod", "0700", runtime});
    std::vector<std::string> sockets{"pipewire-0", "pulse/native"};
    if (!places.gamescopeWayland.empty()) {
        sockets.push_back(places.gamescopeWayland);
    }
    for (const std::string& socket : sockets) {
        const std::string path = (places.runtimeDir / socket).string();
        add(args, {"--bind-try", path, path});
    }
    for (const std::string_view variable : desktopVariables) {
        add(args, {"--unsetenv", variable});
    }
    const std::string bus = (places.runtimeDir / "bus").string();
    add(args, {"--bind", secretBus.string(), bus});
    add(args, {"--setenv", "DBUS_SESSION_BUS_ADDRESS", "unix:path=" + bus});
    add(args, {"--setenv", "DISPLAY", places.display, "--new-session", "--"});
    return args;
}

} // namespace opensu::steam
