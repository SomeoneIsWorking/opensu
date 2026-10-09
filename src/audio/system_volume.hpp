// system_volume — the one owner of the system output volume. It applies the changes openSU's own
// controls make (the Settings rows and the volume shortcuts) and watches for the ones the desktop
// or Gamescope make with the media keys, so every change is told to one listener and none is
// applied twice. Without a mixer it refuses every change with the install advice.
#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>

#include "volume_backend.hpp"

namespace opensu::audio {

/// How much one volume step moves.
inline constexpr int volumeStep = 5;

class SystemVolume {
  public:
    /// Told the volume after every change, whoever made it.
    using Listener = std::function<void(const VolumeState&)>;

    /// `backend` is null when no mixer is installed.
    explicit SystemVolume(std::unique_ptr<VolumeBackend> backend);
    ~SystemVolume();
    SystemVolume(const SystemVolume&) = delete;
    SystemVolume& operator=(const SystemVolume&) = delete;

    void setListener(Listener listener) {
        listener_ = std::move(listener);
    }

    [[nodiscard]] bool available() const noexcept {
        return backend_ != nullptr;
    }
    /// Why there is no volume control; empty when there is.
    [[nodiscard]] std::string unavailable() const;
    /// The last volume read or set; nothing before the first read or without a mixer.
    [[nodiscard]] const std::optional<VolumeState>& state() const noexcept {
        return state_;
    }

    /// Reads the mixer now, telling the listener only of a change from what was known.
    void refresh();
    /// Moves the volume by `delta` percent, clamped to 0 to 100. Empty on success, else why not.
    std::string step(int delta);
    std::string setPercent(int percent);
    std::string toggleMute();
    std::string setMuted(bool muted);

    /// Watches the mixer: reads it in the background once `interval` has passed since the last
    /// read and tells the listener of a change someone else made. Never blocks.
    void poll(std::chrono::steady_clock::time_point now);
    /// Waits for a background read to finish and takes it.
    void finishPoll();

  private:
    /// Settles a background read and reads once if nothing is known yet, leaving the volume in
    /// `current`. Empty when the mixer answered, else why the change is refused.
    [[nodiscard]] std::string prepare(VolumeState& current);
    /// Takes `next` as the volume and tells the listener when it differs.
    void adopt(const std::optional<VolumeState>& next);

    std::unique_ptr<VolumeBackend> backend_;
    std::optional<VolumeState> state_;
    Listener listener_;
    std::future<std::optional<VolumeState>> reading_;
    std::chrono::steady_clock::time_point lastRead_{};
};

/// How often the mixer is read for changes made elsewhere.
inline constexpr std::chrono::milliseconds volumePollInterval{700};

} // namespace opensu::audio
