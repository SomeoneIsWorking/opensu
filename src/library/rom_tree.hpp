// rom_tree — the ROM folders: what the scan walks, and where a ROM taken from elsewhere (LAN
// import) is put. The filesystem, and on Android also the folders the player granted through the
// system picker, named by `content://` document URIs.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
    /// Whether `location` opens as a file, for readers of a game's own data (titles).
    bool local{true};
    /// When it last changed, in the tree's own clock; 0 when the tree does not say.
    std::int64_t modified{0};
};

/// A file of a ROM tree, open for reading at any offset.
class RomFile {
  public:
    RomFile() = default;
    RomFile(const RomFile&) = delete;
    RomFile& operator=(const RomFile&) = delete;
    RomFile(RomFile&&) = delete;
    RomFile& operator=(RomFile&&) = delete;
    virtual ~RomFile() = default;

    [[nodiscard]] virtual std::uint64_t size() const = 0;
    /// The `size` bytes at `offset`; nothing when they cannot all be read.
    [[nodiscard]] virtual std::optional<std::string> read(std::uint64_t offset,
                                                          std::size_t size) = 0;
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
    /// The file a node's `location` names, open for reading; null when it cannot be opened.
    [[nodiscard]] virtual std::unique_ptr<RomFile>
    open(const std::filesystem::path& file) const = 0;
    /// How the player reads `folder`: its path, or a picked folder's name.
    [[nodiscard]] virtual std::string label(const std::filesystem::path& folder) const = 0;
    /// Why files cannot be put in `folder`; empty when they can.
    [[nodiscard]] virtual std::string writeRefusal(const std::filesystem::path& folder) const = 0;
    /// Gives back what the OS keeps granted on `folder`, once openSU no longer uses it.
    virtual void release(const std::filesystem::path& folder) const = 0;
    /// Moves `staged` to `relative` (`/`-separated, checked by the caller) under `folder`, making
    /// the folders on the way and replacing a file there. False with `error` when it cannot.
    [[nodiscard]] virtual bool place(const std::filesystem::path& folder, std::string_view relative,
                                     const std::filesystem::path& staged,
                                     std::string& error) const = 0;
};

/// The filesystem.
class FileTree final : public RomTree {
  public:
    /// The file or folder at `path` as the tree lists it.
    [[nodiscard]] static RomNode node(const std::filesystem::path& path);

    [[nodiscard]] std::optional<std::vector<RomNode>>
    children(const std::filesystem::path& folder) const override;
    [[nodiscard]] std::unique_ptr<RomFile> open(const std::filesystem::path& file) const override;
    [[nodiscard]] std::string label(const std::filesystem::path& folder) const override;
    [[nodiscard]] std::string writeRefusal(const std::filesystem::path& folder) const override;
    void release(const std::filesystem::path& folder) const override;
    [[nodiscard]] bool place(const std::filesystem::path& folder, std::string_view relative,
                             const std::filesystem::path& staged,
                             std::string& error) const override;
};

/// This OS's tree: the filesystem, with picked document folders on Android.
[[nodiscard]] std::unique_ptr<RomTree> openRomTree();

} // namespace opensu::library::roms
