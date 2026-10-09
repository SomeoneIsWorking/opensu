// volume_backend — the system output volume as a program reports and sets it. One interface over
// PipeWire (`wpctl`) and PulseAudio (`pactl`); the programs run through a `Runner`, so tests give
// it canned output and never touch the real mixer.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::audio {

/// The default output's volume and mute.
struct VolumeState {
    /// 0 to 100.
    int percent{0};
    bool muted{false};

    bool operator==(const VolumeState&) const = default;
};

/// One output the mixer can play to.
struct AudioSink {
    /// What the mixer's set-default command takes: wpctl's number, pactl's sink name.
    std::string id;
    /// The name the player reads.
    std::string name;
    bool isDefault{false};

    bool operator==(const AudioSink&) const = default;
};

/// What a finished program said.
struct CommandOutput {
    int status{0};
    std::string output;
};

/// Runs a program with its arguments and returns its stdout, or nothing when it could not run.
using Runner = std::function<std::optional<CommandOutput>(const std::string& program,
                                                          const std::vector<std::string>& args)>;

class VolumeBackend {
  public:
    virtual ~VolumeBackend() = default;

    /// The mixer's name for messages ("wpctl").
    [[nodiscard]] virtual std::string_view name() const = 0;
    /// The default output now, or nothing when the mixer did not answer.
    [[nodiscard]] virtual std::optional<VolumeState> read() = 0;
    /// Sets the volume (0 to 100); whether the mixer accepted it.
    virtual bool setPercent(int percent) = 0;
    virtual bool setMuted(bool muted) = 0;
    /// Every output, in the mixer's order; empty when the mixer did not answer.
    [[nodiscard]] virtual std::vector<AudioSink> sinks() = 0;
    /// Makes sink `id` the default output; whether the mixer accepted it.
    virtual bool setDefaultSink(const std::string& id) = 0;
};

/// PipeWire's `wpctl`.
[[nodiscard]] std::unique_ptr<VolumeBackend> makeWpctl(Runner run);
/// PulseAudio's (and pipewire-pulse's) `pactl`.
[[nodiscard]] std::unique_ptr<VolumeBackend> makePactl(Runner run);

/// The mixers `exists` says are installed, `wpctl` first. Nothing when neither is.
[[nodiscard]] std::unique_ptr<VolumeBackend>
detectBackend(Runner run, const std::function<bool(const std::string& program)>& exists);

/// Wraps `inner` so nothing is written to the system: it reads the volume once and then holds the
/// changes itself. For hidden runs that must leave the player's mixer alone.
[[nodiscard]] std::unique_ptr<VolumeBackend> makeReadOnly(std::unique_ptr<VolumeBackend> inner);

/// What to tell the player when no mixer is installed, with the install command per platform.
[[nodiscard]] std::string missingBackendAdvice();

/// `wpctl status` output as the sinks of its Audio section.
[[nodiscard]] std::vector<AudioSink> parseWpctlSinks(std::string_view status);
/// `pactl --format=json list sinks` output, with the default sink's name, as sinks.
[[nodiscard]] std::vector<AudioSink> parsePactlSinks(std::string_view json,
                                                     std::string_view defaultName);

/// `wpctl get-volume` output ("Volume: 0.45 [MUTED]") as a state.
[[nodiscard]] std::optional<VolumeState> parseWpctl(std::string_view output);
/// `pactl get-sink-volume` and `get-sink-mute` output as a state.
[[nodiscard]] std::optional<VolumeState> parsePactl(std::string_view volume, std::string_view mute);

} // namespace opensu::audio
