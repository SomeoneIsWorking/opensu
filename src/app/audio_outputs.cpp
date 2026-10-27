#include "audio_outputs.hpp"

#include <algorithm>

namespace opensu::app {

void AudioOutputs::refresh() {
    sinks_ = volume_.sinks();
}

const audio::AudioSink* AudioOutputs::current() const {
    const auto found = std::ranges::find(sinks_, true, &audio::AudioSink::isDefault);
    return found == sinks_.end() ? nullptr : &*found;
}

std::string AudioOutputs::choose(const std::string& id) {
    std::string refused = volume_.setDefaultSink(id);
    refresh();
    return refused;
}

std::string AudioOutputs::unavailable() const {
    return volume_.available() ? std::string{} : volume_.unavailable();
}

} // namespace opensu::app
