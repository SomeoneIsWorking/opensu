#include "system_volume.hpp"

#include <algorithm>

namespace opensu::audio {

SystemVolume::SystemVolume(std::unique_ptr<VolumeBackend> backend) : backend_{std::move(backend)} {
}

SystemVolume::~SystemVolume() {
    if (reading_.valid()) {
        reading_.wait();
    }
}

std::string SystemVolume::unavailable() const {
    return available() ? std::string{} : missingBackendAdvice();
}

void SystemVolume::adopt(const std::optional<VolumeState>& next) {
    if (!next || next == state_) {
        return;
    }
    state_ = next;
    if (listener_) {
        listener_(*state_);
    }
}

void SystemVolume::refresh() {
    if (backend_) {
        finishPoll();
        adopt(backend_->read());
    }
}

std::string SystemVolume::prepare(VolumeState& current) {
    if (!backend_) {
        return missingBackendAdvice();
    }
    finishPoll();
    if (!state_) {
        adopt(backend_->read());
    }
    if (!state_) {
        return std::string{backend_->name()} + " did not answer";
    }
    current = *state_;
    return {};
}

std::string SystemVolume::setPercent(int percent) {
    VolumeState current;
    if (std::string refused = prepare(current); !refused.empty()) {
        return refused;
    }
    const int next = std::clamp(percent, 0, 100);
    if (!backend_->setPercent(next)) {
        return std::string{backend_->name()} + " refused the volume";
    }
    VolumeState changed = current;
    changed.percent = next;
    adopt(changed);
    return {};
}

std::string SystemVolume::step(int delta) {
    VolumeState current;
    if (std::string refused = prepare(current); !refused.empty()) {
        return refused;
    }
    return setPercent(current.percent + delta);
}

std::string SystemVolume::setMuted(bool muted) {
    VolumeState current;
    if (std::string refused = prepare(current); !refused.empty()) {
        return refused;
    }
    if (!backend_->setMuted(muted)) {
        return std::string{backend_->name()} + " refused the mute";
    }
    VolumeState changed = current;
    changed.muted = muted;
    adopt(changed);
    return {};
}

std::string SystemVolume::toggleMute() {
    VolumeState current;
    if (std::string refused = prepare(current); !refused.empty()) {
        return refused;
    }
    return setMuted(!current.muted);
}

std::vector<AudioSink> SystemVolume::sinks() {
    if (!backend_) {
        return {};
    }
    finishPoll();
    return backend_->sinks();
}

std::string SystemVolume::setDefaultSink(const std::string& id) {
    if (!backend_) {
        return missingBackendAdvice();
    }
    finishPoll();
    if (!backend_->setDefaultSink(id)) {
        return std::string{backend_->name()} + " refused the output";
    }
    adopt(backend_->read());
    return {};
}

void SystemVolume::finishPoll() {
    if (reading_.valid()) {
        adopt(reading_.get());
    }
}

void SystemVolume::poll(std::chrono::steady_clock::time_point now) {
    if (!backend_) {
        return;
    }
    if (reading_.valid()) {
        if (reading_.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
            adopt(reading_.get());
        }
        return;
    }
    if (now - lastRead_ < volumePollInterval) {
        return;
    }
    lastRead_ = now;
    reading_ = std::async(std::launch::async, [this] {
        return backend_->read();
    });
}

} // namespace opensu::audio
