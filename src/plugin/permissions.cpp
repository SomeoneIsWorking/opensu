#include "permissions.hpp"

#include <algorithm>
#include <system_error>

namespace opensu::plugin {
namespace {

namespace fs = std::filesystem;

/// `path` with symlinks and `..` resolved as far as it exists; empty when it cannot be.
fs::path resolved(const fs::path& path) {
    std::error_code ec;
    fs::path out = fs::weakly_canonical(path, ec);
    return ec ? fs::path{} : out;
}

bool within(const fs::path& path, const fs::path& root) {
    auto part = path.begin();
    for (auto rootPart = root.begin(); rootPart != root.end(); ++rootPart, ++part) {
        if (rootPart->empty()) {
            continue;
        }
        if (part == path.end() || *part != *rootPart) {
            return false;
        }
    }
    return true;
}

std::string lowerAscii(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return out;
}

} // namespace

Permissions::Permissions(const Manifest& manifest, const PathTokens& tokens)
    : hosts_{manifest.http} {
    for (const auto& [group, patterns] : manifest.read) {
        std::vector<fs::path>& paths = roots_[group];
        for (const std::string& pattern : patterns) {
            if (std::optional<fs::path> path = expandPath(pattern, tokens)) {
                paths.push_back(path->lexically_normal());
            }
        }
    }
}

std::map<std::string, std::vector<fs::path>> Permissions::existingRoots() const {
    std::map<std::string, std::vector<fs::path>> existing;
    for (const auto& [group, paths] : roots_) {
        std::vector<fs::path>& found = existing[group];
        for (const fs::path& path : paths) {
            std::error_code ec;
            if (fs::is_directory(path, ec)) {
                if (fs::path root = resolved(path);
                    !root.empty() && std::ranges::find(found, root) == found.end()) {
                    found.push_back(std::move(root));
                }
            }
        }
    }
    return existing;
}

std::optional<fs::path> Permissions::readable(const fs::path& path, std::string& error) const {
    if (!path.is_absolute()) {
        error = path.string() + " is not an absolute path";
        return std::nullopt;
    }
    fs::path target = resolved(path);
    for (const auto& [group, paths] : roots_) {
        for (const fs::path& root : paths) {
            const fs::path resolvedRoot = resolved(root);
            if (!target.empty() && !resolvedRoot.empty() && within(target, resolvedRoot)) {
                return target;
            }
        }
    }
    error = path.string() + " is outside the plugin's read paths";
    return std::nullopt;
}

std::string Permissions::urlRefusal(std::string_view url) const {
    constexpr std::string_view scheme = "https://";
    if (!lowerAscii(url.substr(0, scheme.size())).starts_with(scheme)) {
        return std::string{url} + " is not HTTPS";
    }
    const std::string_view rest = url.substr(scheme.size());
    const std::string_view authority = rest.substr(0, rest.find_first_of("/?#"));
    if (authority.find('@') != std::string_view::npos) {
        return std::string{url} + " carries credentials";
    }
    const std::string host = lowerAscii(authority.substr(0, authority.find(':')));
    if (std::ranges::find(hosts_, host) == hosts_.end()) {
        return host + " is not a host the plugin declared";
    }
    return {};
}

std::vector<fs::path> Permissions::readPaths() const {
    std::vector<fs::path> all;
    for (const auto& [group, paths] : roots_) {
        for (const fs::path& path : paths) {
            if (std::ranges::find(all, path) == all.end()) {
                all.push_back(path);
            }
        }
    }
    return all;
}

} // namespace opensu::plugin
