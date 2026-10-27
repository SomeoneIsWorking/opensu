#include "screen_images.hpp"

#include <algorithm>

namespace opensu::ui {

void ScreenImages::want(const std::filesystem::path& file) {
    if (file.empty()) {
        return;
    }
    const auto [slot, added] = slots_.try_emplace(file);
    if (added) {
        slot->second.id = next_++;
        decoder_.request(DecodeJob{generation_, slot->second.id, 0, file, {}});
    }
}

const gpu2d::Texture* ScreenImages::find(const std::filesystem::path& file) const {
    const auto slot = slots_.find(file);
    if (slot == slots_.end()) {
        return nullptr;
    }
    const std::optional<gpu2d::Texture>& texture = slot->second.texture;
    return texture ? &*texture : nullptr;
}

void ScreenImages::load(const Upload& upload) {
    for (const Decoded& decoded : decoder_.take(slots_.size())) {
        const DecodeJob& job = decoded.job;
        const auto slot = slots_.find(job.portrait);
        if (job.shelf != generation_ || slot == slots_.end() || slot->second.id != job.tile ||
            decoded.portrait.rgba.empty()) {
            continue;
        }
        slot->second.texture = upload(decoded.portrait);
    }
}

void ScreenImages::clear() {
    if (slots_.empty()) {
        return;
    }
    slots_.clear();
    ++generation_;
    static_cast<void>(decoder_.prune([](const DecodeJob&) {
        return false;
    }));
}

} // namespace opensu::ui
