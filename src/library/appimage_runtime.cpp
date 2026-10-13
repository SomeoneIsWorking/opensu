#include "appimage_runtime.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>

#include <elf.h>

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view libfuse2Name = "libfuse.so.2";
/// No runtime is larger; past this the file is the filesystem image.
constexpr std::uint64_t runtimeLimit = 8ULL * 1024 * 1024;

constexpr std::array libraryDirs{
    "/usr/lib64",           "/lib64", "/usr/lib", "/lib", "/usr/lib/x86_64-linux-gnu",
    "/lib/x86_64-linux-gnu"};

bool hasWord(std::string_view words, std::string_view word) {
    std::size_t at = 0;
    while (at < words.size()) {
        const std::size_t end = std::min(words.find(' ', at), words.size());
        if (words.substr(at, end - at) == word) {
            return true;
        }
        at = end + 1;
    }
    return false;
}

std::string osReleaseValue(const std::string& line, std::string_view key) {
    std::string value = line.substr(key.size() + 1);
    std::erase(value, '"');
    return value;
}

/// Where the runtime ends: its section headers are the last thing in it.
std::uint64_t runtimeSize(std::ifstream& in) {
    Elf64_Ehdr header{};
    in.read(reinterpret_cast<char*>(&header), sizeof header);
    if (!in || std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS64) {
        return 0;
    }
    return header.e_shoff + static_cast<std::uint64_t>(header.e_shentsize) * header.e_shnum;
}

} // namespace

FuseSupport FuseSupport::detect(const std::vector<fs::path>& executablePath) {
    FuseSupport support;
    std::error_code ec;
    support.libfuse2 = std::ranges::any_of(libraryDirs, [&ec](const char* dir) {
        return fs::exists(fs::path{dir} / libfuse2Name, ec);
    });
    support.fusermount = std::ranges::any_of(executablePath, [&ec](const fs::path& dir) {
        return fs::exists(dir / "fusermount", ec) || fs::exists(dir / "fusermount3", ec);
    });
    support.device = fs::exists("/dev/fuse", ec);
    std::ifstream release{"/etc/os-release"};
    std::string line;
    while (std::getline(release, line)) {
        if (line.starts_with("ID=") || line.starts_with("ID_LIKE=")) {
            const std::string_view key = line.starts_with("ID=") ? "ID" : "ID_LIKE";
            support.distro += (support.distro.empty() ? "" : " ") + osReleaseValue(line, key);
        }
    }
    return support;
}

bool needsLibfuse2(const fs::path& appImage) {
    std::ifstream in{appImage, std::ios::binary};
    const std::uint64_t size = std::min(runtimeSize(in), runtimeLimit);
    if (size == 0) {
        return false;
    }
    std::string runtime(size, '\0');
    in.seekg(0);
    in.read(runtime.data(), static_cast<std::streamsize>(size));
    return runtime.find(libfuse2Name) != std::string::npos;
}

std::string libfuse2Install(std::string_view distro) {
    if (hasWord(distro, "fedora") || hasWord(distro, "rhel")) {
        return "sudo dnf install fuse-libs";
    }
    if (hasWord(distro, "ubuntu") || hasWord(distro, "debian")) {
        return "sudo apt install libfuse2 (libfuse2t64 on Ubuntu 24.04 and later)";
    }
    if (hasWord(distro, "arch")) {
        return "sudo pacman -S fuse2";
    }
    if (hasWord(distro, "suse")) {
        return "sudo zypper install libfuse2";
    }
    return "install your distribution's libfuse2 package";
}

std::string appImageRefusal(const fs::path& appImage, const FuseSupport& fuse) {
    if (!fuse.device) {
        return "AppImages need FUSE, and this system has no /dev/fuse";
    }
    if (!fuse.fusermount) {
        return "AppImages need fusermount; " +
               std::string{hasWord(fuse.distro, "fedora") ? "sudo dnf install fuse3"
                                                          : "install your distribution's fuse3"};
    }
    if (!fuse.libfuse2 && needsLibfuse2(appImage)) {
        return "this AppImage needs libfuse2: " + libfuse2Install(fuse.distro);
    }
    return {};
}

} // namespace opensu::library::roms
