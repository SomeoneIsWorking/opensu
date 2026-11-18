#include "mac_bundle.hpp"

#include <fstream>
#include <iterator>
#include <utility>

namespace opensu::library {

namespace fs = std::filesystem;

MacBundle::MacBundle(fs::path app) : root_{std::move(app)} {
    std::ifstream in{root_ / "Contents" / "Info.plist", std::ios::binary};
    plist_.assign(std::istreambuf_iterator<char>{in}, {});
}

fs::path MacBundle::containing(const fs::path& program) {
    for (fs::path folder = program; folder.has_relative_path(); folder = folder.parent_path()) {
        if (folder.extension() == ".app") {
            return folder;
        }
    }
    return {};
}

std::string MacBundle::value(std::string_view key) const {
    const std::string tag = "<key>" + std::string{key} + "</key>";
    const std::size_t at = plist_.find(tag);
    if (at == std::string::npos) {
        return {};
    }
    constexpr std::string_view open = "<string>";
    const std::size_t from = plist_.find_first_not_of(" \t\r\n", at + tag.size());
    if (from == std::string::npos || plist_.compare(from, open.size(), open) != 0) {
        return {};
    }
    const std::size_t close = plist_.find("</string>", from);
    if (close == std::string::npos) {
        return {};
    }
    return plist_.substr(from + open.size(), close - from - open.size());
}

fs::path MacBundle::executable() const {
    std::string program = value("CFBundleExecutable");
    if (program.empty()) {
        program = root_.stem().string();
    }
    return root_ / "Contents" / "MacOS" / program;
}

std::string MacBundle::title() const {
    for (const std::string_view key : {"CFBundleDisplayName", "CFBundleName"}) {
        if (std::string name = value(key); !name.empty()) {
            return name;
        }
    }
    return root_.stem().string();
}

fs::path MacBundle::iconFile() const {
    fs::path named{value("CFBundleIconFile")};
    if (named.empty()) {
        return {};
    }
    if (!named.has_extension()) {
        named += ".icns";
    }
    return root_ / "Contents" / "Resources" / named.filename();
}

} // namespace opensu::library
