#include "cover_picker_art.hpp"

namespace opensu::ui {

void CoverPickerArt::load(const CoverPicker& picker, const Upload& upload) {
    if (!picker.isOpen()) {
        slots_.clear();
        return;
    }
    if (picker.generation() != generation_) {
        generation_ = picker.generation();
        slots_.clear();
    }
    const std::vector<CoverPick>& picks = picker.picks();
    slots_.resize(picks.size());
    for (std::size_t i = 0; i < picks.size(); ++i) {
        Slot& slot = slots_[i];
        if (slot.file != picks[i].file) {
            slot = Slot{picks[i].file, false, std::nullopt};
        }
        if (!slot.file.empty() && !slot.requested) {
            decoder_.request(DecodeJob{generation_, i, 0, slot.file, {}});
            slot.requested = true;
        }
    }
    for (const Decoded& decoded : decoder_.take(slots_.size())) {
        const DecodeJob& job = decoded.job;
        if (job.shelf != generation_ || job.tile >= slots_.size() ||
            slots_[job.tile].file != job.portrait || decoded.portrait.rgba.empty()) {
            continue;
        }
        slots_[job.tile].texture = upload(decoded.portrait);
    }
}

const gpu2d::Texture* CoverPickerArt::find(std::size_t index) const {
    if (index >= slots_.size()) {
        return nullptr;
    }
    const std::optional<gpu2d::Texture>& texture = slots_[index].texture;
    return texture ? &*texture : nullptr;
}

} // namespace opensu::ui
