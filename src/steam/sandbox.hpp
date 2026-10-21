// sandbox — the bwrap layer Isolated Steam runs in: the player's own files, home and login, with
// openSU's Gamescope Xwayland as its only display, an empty runtime dir holding only Gamescope's
// and the audio sockets (no desktop Wayland; a session bus that reaches only the Secret Service,
// `secret_bus.hpp`), a private /tmp and
// /dev/shm, the GPU, and only the input nodes it is given. The host's /proc stays, read-write, so
// the pressure-vessel containers Steam nests inside can map their uids; the pid and network
// namespaces are the host's, so the client's DevTools, its pid file, `steam -applaunch` and
// `steam -shutdown` work from outside as for Managed.
#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::steam {

/// The fixed virtual pads Isolated Steam gets in place of the player's controllers: XInput's four.
/// Nodes are bound when the sandbox starts, so they exist for as long as Steam runs.
inline constexpr std::size_t isolatedPadSlots{4};

/// Where the sandbox finds the session it lets Steam reach.
struct SandboxPlaces {
    /// The player's XDG_RUNTIME_DIR; Steam gets an empty one in its place.
    std::filesystem::path runtimeDir;
    /// openSU's Gamescope Xwayland, `:1`.
    std::string display;
    /// Gamescope's Wayland socket name in the runtime dir, for the games' WSI layer; may be empty.
    std::string gamescopeWayland;
    /// The GPU's device nodes (`gpuNodes`).
    std::vector<std::filesystem::path> gpu;
};

/// The controllers the sandbox has.
struct SandboxDevices {
    /// The host's /dev/input, live, and udev's database.
    bool allInput{false};
    /// Otherwise the only input nodes, and no udev database.
    std::vector<std::filesystem::path> inputNodes;
};

/// The number of a local X display, `:1` or `:1.0`; nothing for anything else.
[[nodiscard]] std::optional<int> x11DisplayNumber(std::string_view display) noexcept;

/// `/dev/dri` and every `/dev/nvidia*` under `dev`.
[[nodiscard]] std::vector<std::filesystem::path> gpuNodes(const std::filesystem::path& dev);

/// bwrap's arguments for Steam, ending in `--`, with `secretBus` (`SecretBus`'s socket) as the
/// sandbox's session bus; empty with `failure` set when `places.display` is not a local X
/// display.
[[nodiscard]] std::vector<std::string> sandboxArguments(const SandboxPlaces& places,
                                                        const SandboxDevices& devices,
                                                        const std::filesystem::path& secretBus,
                                                        std::string& failure);

} // namespace opensu::steam
