// rom_tree — the folders the ROM scan walks: the filesystem, and on Android also the folders the
// player granted through the system picker, named by `content://` document URIs.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace opensu::library::roms {

/// One file or folder in a ROM tree.
struct RomNode {
    /// What a launch and the next listing take: a path, or a document URI.
    std::filesystem::path location;
    /// The name the player gave it, extension included.
    std::string name;
    bool folder{false};
    std::uintmax_t size{0};
    /// Whether `location` opens as a file, for readers of a game's own data (titles, romsets).
    bool local{true};
};

class RomTree {
  public:
    RomTree() = default;
    RomTree(const RomTree&) = delete;
    RomTree& operator=(const RomTree&) = delete;
    RomTree(RomTree&&) = delete;
    RomTree& operator=(RomTree&&) = delete;
    virtual ~RomTree() = default;

    /// What `folder` holds, sorted by name; nothing when it is no folder the app can read.
    [[nodiscard]] virtual std::optional<std::vector<RomNode>>
    children(const std::filesystem::path& folder) const = 0;
    /// How the player reads `folder`: its path, or a picked folder's name.
    [[nodiscard]] virtual std::string label(const std::filesystem::path& folder) const = 0;
};

/// The filesystem.
class FileTree final : public RomTree {
  public:
    /// The file or folder at `path` as the tree lists it.
    [[nodiscard]] static RomNode node(const std::filesystem::path& path);

    [[nodiscard]] std::optional<std::vector<RomNode>>
    children(const std::filesystem::path& folder) const override;
    [[nodiscard]] std::string label(const std::filesystem::path& folder) const override;
};

/// This OS's tree: the filesystem, with picked document folders on Android.
[[nodiscard]] std::unique_ptr<RomTree> openRomTree();

} // namespace opensu::library::roms
