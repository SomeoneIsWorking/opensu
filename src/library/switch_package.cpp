#include "switch_package.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <system_error>

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

constexpr std::size_t partitionHeader = 0x10;
constexpr std::size_t pfs0Entry = 0x18;
constexpr std::size_t hfs0Entry = 0x40;
/// Far more files or name bytes than any package holds means the header is not one.
constexpr std::uint32_t mostFiles = 0x4000;
constexpr std::uint32_t mostNameBytes = 0x100000;

/// The card header ("HEAD" at 0x100) and where the root HFS0 is named in it.
constexpr std::size_t cardHeader = 0x200;
constexpr std::size_t cardMagicAt = 0x100;
constexpr std::size_t rootPartitionAt = 0x130;
/// A full card image carries its key area before the header.
constexpr std::uint64_t keyAreaSize = 0x1000;

/// A ticket's signature sizes by type, with the padding to the next 0x40 bytes.
constexpr std::uint32_t rsa4096Sha1 = 0x10000;
constexpr std::uint32_t rsa2048Sha1 = 0x10001;
constexpr std::uint32_t ecdsaSha1 = 0x10002;
constexpr std::uint32_t rsa4096Sha256 = 0x10003;
constexpr std::uint32_t rsa2048Sha256 = 0x10004;
constexpr std::uint32_t ecdsaSha256 = 0x10005;
constexpr std::size_t ticketTitleKeyAt = 0x40;
constexpr std::size_t ticketKeyTypeAt = 0x141;
constexpr std::size_t ticketRightsAt = 0x160;
constexpr std::size_t ticketBody = 0x180;
constexpr std::size_t largestTicket = 0x1000;

bool magicIs(std::span<const std::uint8_t> bytes, std::size_t at, std::string_view magic) {
    return bytes.size() >= at + magic.size() &&
           std::equal(magic.begin(), magic.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at),
                      [](char a, std::uint8_t b) {
                          return static_cast<std::uint8_t>(a) == b;
                      });
}

std::optional<std::size_t> ticketHeaderAt(std::uint32_t signature) {
    switch (signature) {
    case rsa4096Sha1:
    case rsa4096Sha256:
        return 4 + 0x200 + 0x3C;
    case rsa2048Sha1:
    case rsa2048Sha256:
        return 4 + 0x100 + 0x3C;
    case ecdsaSha1:
    case ecdsaSha256:
        return 4 + 0x3C + 0x40;
    default:
        return std::nullopt;
    }
}

} // namespace

SwitchPackage::SwitchPackage(std::ifstream in, std::uint64_t size)
    : in_{std::move(in)}, size_{size} {
}

std::optional<SwitchPackage> SwitchPackage::open(const fs::path& file, std::string& error) {
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(file, ec);
    if (ec) {
        error = "cannot be read: " + ec.message();
        return std::nullopt;
    }
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        error = "cannot be opened";
        return std::nullopt;
    }
    SwitchPackage package{std::move(in), size};
    std::array<std::uint8_t, 4> magic{};
    if (!package.read(0, magic)) {
        error = "too short to be an NSP or XCI";
        return std::nullopt;
    }
    if (magicIs(magic, 0, "PFS0")) {
        if (!package.readPartition(0, false, package.files_, error)) {
            return std::nullopt;
        }
        return package;
    }
    if (!package.openCard(error)) {
        return std::nullopt;
    }
    return package;
}

bool SwitchPackage::read(std::uint64_t offset, std::span<std::uint8_t> out) {
    if (offset > size_ || out.size() > size_ - offset) {
        return false;
    }
    in_.clear();
    in_.seekg(static_cast<std::streamoff>(offset));
    in_.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return static_cast<std::size_t>(in_.gcount()) == out.size();
}

bool SwitchPackage::readPartition(std::uint64_t at, bool hashed, std::vector<PackageFile>& out,
                                  std::string& error) {
    const std::string_view magic = hashed ? "HFS0" : "PFS0";
    std::array<std::uint8_t, partitionHeader> header{};
    if (!read(at, header) || !magicIs(header, 0, magic)) {
        error = "no " + std::string{magic} + " partition where the header says";
        return false;
    }
    const auto count = littleEndian<std::uint32_t>(header, 4);
    const auto nameBytes = littleEndian<std::uint32_t>(header, 8);
    if (count > mostFiles || nameBytes > mostNameBytes) {
        error = std::string{magic} + " header is damaged";
        return false;
    }
    const std::size_t entrySize = hashed ? hfs0Entry : pfs0Entry;
    std::vector<std::uint8_t> table(count * entrySize + nameBytes);
    if (!read(at + partitionHeader, table)) {
        error = std::string{magic} + " file table is cut short";
        return false;
    }
    const std::uint64_t dataStart = at + partitionHeader + table.size();
    const std::span<const std::uint8_t> names{table.data() + count * entrySize, nameBytes};
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t entry = i * entrySize;
        PackageFile file;
        file.offset = dataStart + littleEndian<std::uint64_t>(table, entry);
        file.size = littleEndian<std::uint64_t>(table, entry + 8);
        const auto nameAt = littleEndian<std::uint32_t>(table, entry + 16);
        if (nameAt >= names.size() || file.offset > size_ || file.size > size_ - file.offset) {
            error = std::string{magic} + " names a file outside the game file";
            return false;
        }
        const auto end = std::find(names.begin() + nameAt, names.end(), std::uint8_t{0});
        file.name.assign(names.begin() + nameAt, end);
        out.push_back(std::move(file));
    }
    return true;
}

bool SwitchPackage::openCard(std::string& error) {
    std::array<std::uint8_t, cardHeader> header{};
    std::uint64_t base = 0;
    if (!read(base, header) || !magicIs(header, cardMagicAt, "HEAD")) {
        base = keyAreaSize;
        if (!read(base, header) || !magicIs(header, cardMagicAt, "HEAD")) {
            error = "neither an NSP (PFS0) nor an XCI (card header)";
            return false;
        }
    }
    std::vector<PackageFile> root;
    if (!readPartition(base + littleEndian<std::uint64_t>(header, rootPartitionAt), true, root,
                       error)) {
        return false;
    }
    const auto secure = std::ranges::find(root, std::string{"secure"}, &PackageFile::name);
    if (secure == root.end()) {
        error = "XCI has no secure partition";
        return false;
    }
    return readPartition(secure->offset, true, files_, error);
}

std::optional<Key128> SwitchPackage::ticketTitleKey(const RightsId& rights) {
    for (const PackageFile& file : files_) {
        if (!file.name.ends_with(".tik") || file.size < 4 || file.size > largestTicket) {
            continue;
        }
        std::vector<std::uint8_t> ticket(file.size);
        if (!read(file.offset, ticket)) {
            continue;
        }
        const std::optional<std::size_t> at =
            ticketHeaderAt(littleEndian<std::uint32_t>(ticket, 0));
        if (!at || ticket.size() < *at + ticketBody) {
            continue;
        }
        const std::span<const std::uint8_t> body{ticket.data() + *at, ticketBody};
        // Type 0 is a common ticket; a personalised one is encrypted to the console.
        if (body[ticketKeyTypeAt] != 0 ||
            !std::equal(rights.begin(), rights.end(),
                        body.begin() + static_cast<std::ptrdiff_t>(ticketRightsAt))) {
            continue;
        }
        Key128 key;
        std::copy_n(body.begin() + static_cast<std::ptrdiff_t>(ticketTitleKeyAt),
                    key.bytes().size(), key.bytes().begin());
        wipeBytes(ticket.data(), ticket.size());
        return key;
    }
    return std::nullopt;
}

} // namespace opensu::library::roms
