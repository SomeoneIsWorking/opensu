#include "save_share_kind.hpp"

#include <algorithm>

namespace opensu::settings {

std::string_view key(SaveShareKind kind) noexcept {
    switch (kind) {
    case SaveShareKind::Off:
        return "off";
    case SaveShareKind::Folder:
        return "folder";
    case SaveShareKind::WebDav:
        return "webdav";
    }
    return {};
}

std::optional<SaveShareKind> saveShareKindOf(std::string_view spelling) noexcept {
    const auto found = std::ranges::find_if(allSaveShareKinds, [spelling](SaveShareKind kind) {
        return key(kind) == spelling;
    });
    return found == allSaveShareKinds.end() ? std::nullopt : std::optional{*found};
}

std::string_view label(SaveShareKind kind) noexcept {
    switch (kind) {
    case SaveShareKind::Off:
        return "Off";
    case SaveShareKind::Folder:
        return "A folder";
    case SaveShareKind::WebDav:
        return "WebDAV";
    }
    return {};
}

} // namespace opensu::settings
