#include "iisu_install.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <system_error>

#include "lucent/log.h"

namespace opensu::artwork {
namespace {

bool packLike(const std::filesystem::path& file) {
    std::string extension = file.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".apk" || extension == ".zip";
}

} // namespace

std::vector<IisuInstall::Placed> IisuInstall::placed() const {
    std::vector<Placed> found;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator{folder_, ec}) {
        std::error_code entryEc;
        if (!entry.is_regular_file(entryEc) || !packLike(entry.path())) {
            continue;
        }
        Placed file{.file = entry.path(),
                    .size = entry.file_size(entryEc),
                    .written = entry.last_write_time(entryEc)};
        found.push_back(std::move(file));
    }
    std::ranges::sort(found, {}, &Placed::file);
    return found;
}

std::shared_ptr<const IisuPack> IisuInstall::pack() const {
    std::vector<Placed> now = placed();
    const std::scoped_lock lock{mutex_};
    if (read_ == now) {
        return pack_;
    }
    std::shared_ptr<const IisuPack> chosen;
    for (const Placed& file : now) {
        std::string error;
        std::shared_ptr<const IisuPack> pack = IisuPack::open(file.file, error);
        if (pack == nullptr) {
            lucent::warn("artwork", "{}: {}; not offering iiSU's art or sounds from it",
                         folder_.string(), error);
        } else if (chosen == nullptr || (pack->hasSounds() && !chosen->hasSounds())) {
            chosen = std::move(pack);
        }
    }
    if (chosen != nullptr) {
        lucent::info("artwork", "iiSU's {} from {}", chosen->hasSounds() ? "art and sounds" : "art",
                     chosen->file().string());
    }
    read_ = std::move(now);
    pack_ = std::move(chosen);
    return pack_;
}

} // namespace opensu::artwork
