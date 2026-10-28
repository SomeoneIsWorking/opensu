#include "appimage_runtime.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view libfuse2Name = "libfuse.so.2";
/// No runtime is larger; past this the file is the filesystem image.
constexpr std::uint64_t runtimeLimit = 8ULL * 1024 * 1024;

// The ELF64 header: magic, class, and the section header table's offset, entry size and count.
constexpr std::size_t elfHeaderSize = 64;
constexpr std::array<unsigned char, 4> elfMagic{0x7F, 'E', 'L', 'F'};
constexpr std::size_t elfClassAt = 4;
constexpr unsigned char elfClass64 = 2;
constexpr std::size_t sectionsAt = 0x28;
constexpr std::size_t sectionSizeAt = 0x3A;
constexpr std::size_t sectionCountAt = 0x3C;

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

/// The little-endian unsigned integer of `size` bytes at `at`.
std::uint64_t littleEndian(const std::array<unsigned char, elfHeaderSize>& bytes, std::size_t at,
                           std::size_t size) {
    std::uint64_t value = 0;
    for (std::size_t i = size; i > 0; --i) {
        value = (value << 8U) | bytes[at + i - 1];
    }
    return value;
}

/// Where the runtime ends: its section headers are the last thing in it.
std::uint64_t runtimeSize(std::ifstream& in) {
    std::array<unsigned char, elfHeaderSize> header{};
    in.read(reinterpret_cast<char*>(header.data()), header.size());
    if (!in || std::memcmp(header.data(), elfMagic.data(), elfMagic.size()) != 0 ||
        header[elfClassAt] != elfClass64) {
        return 0;
    }
    return littleEndian(header, sectionsAt, 8) +
           littleEndian(header, sectionSizeAt, 2) * littleEndian(header, sectionCountAt, 2);
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
