// The system volume owner over fake mixers: parsing the two mixers' output, the commands they get,
// the one-listener rule for own and foreign changes, the read-only wrapper and the refusal without
// a mixer. No real mixer is ever run.
#include <cstdio>
#include <string>
#include <vector>

#include "system_volume.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu::audio;
using opensu::test::expect;
using opensu::test::need;

/// A mixer that keeps its state and records the commands it was given.
struct FakeMixer {
    VolumeState state{40, false};
    std::vector<std::string> commands;
    bool healthy{true};

    Runner runner() {
        return [this](const std::string& program,
                      const std::vector<std::string>& args) -> std::optional<CommandOutput> {
            std::string line = program;
            for (const std::string& arg : args) {
                line += " " + arg;
            }
            commands.push_back(line);
            if (!healthy) {
                return std::nullopt;
            }
            const std::string& verb = args.front();
            if (verb == "get-volume") {
                return CommandOutput{0, "Volume: " + std::to_string(state.percent / 100.0) +
                                            (state.muted ? " [MUTED]\n" : "\n")};
            }
            if (verb == "get-sink-volume") {
                return CommandOutput{0, "Volume: front-left: 1 /  " +
                                            std::to_string(state.percent) +
                                            "% / -3 dB,   front-right: 1 /  " +
                                            std::to_string(state.percent) + "% / -3 dB\n"};
            }
            if (verb == "get-sink-mute") {
                return CommandOutput{0,
                                     std::string{"Mute: "} + (state.muted ? "yes" : "no") + "\n"};
            }
            if (verb == "set-volume" || verb == "set-sink-volume") {
                state.percent = std::stoi(args.back());
            } else if (verb == "set-mute" || verb == "set-sink-mute") {
                state.muted = args.back() == "1";
            }
            return CommandOutput{0, {}};
        };
    }
};

void parsing() {
    expect(parseWpctl("Volume: 0.45\n") == VolumeState{45, false}, "wpctl volume");
    expect(parseWpctl("Volume: 0.00 [MUTED]\n") == VolumeState{0, true}, "wpctl mute");
    expect(need(parseWpctl("Volume: 1.52\n"), "a state").percent == 100,
           "an over-amplified level clamps");
    expect(!parseWpctl("nothing"), "wpctl noise is no state");
    expect(parsePactl("Volume: front-left: 26214 /  40% / -23.88 dB,   front-right: 26214 /  40% "
                      "/ -23.88 dB\n        balance 0.00\n",
                      "Mute: no\n") == VolumeState{40, false},
           "pactl volume");
    expect(need(parsePactl("Volume: mono: 1 / 7% / -1 dB", "Mute: yes"), "a state").muted,
           "pactl mute");
    expect(!parsePactl("garbage", "Mute: no"), "pactl noise is no state");
}

void backendsSpeakTheirMixer() {
    FakeMixer mixer;
    const auto wpctl = makeWpctl(mixer.runner());
    expect(wpctl->read() == VolumeState{40, false}, "wpctl reads");
    expect(wpctl->setPercent(55) && mixer.state.percent == 55, "wpctl sets the volume");
    expect(wpctl->setMuted(true) && mixer.state.muted, "wpctl mutes");
    expect(mixer.commands[1] == "wpctl set-volume @DEFAULT_AUDIO_SINK@ 55%", "the exact command");
    FakeMixer pulse;
    const auto pactl = makePactl(pulse.runner());
    expect(pactl->read() == VolumeState{40, false}, "pactl reads");
    expect(pactl->setPercent(250) && pulse.state.percent == 100, "pactl clamps what it sets");
    pulse.healthy = false;
    expect(!pactl->read() && !pactl->setMuted(true), "a mixer that does not run is refused");
}

void detectionPrefersWpctl() {
    FakeMixer mixer;
    expect(detectBackend(
               mixer.runner(),
               [](const std::string& p) {
                   return p == "wpctl" || p == "pactl";
               })->name() == "wpctl",
           "wpctl first");
    expect(detectBackend(
               mixer.runner(),
               [](const std::string& p) {
                   return p == "pactl";
               })->name() == "pactl",
           "pactl when it is all there is");
    expect(detectBackend(mixer.runner(),
                         [](const std::string&) {
                             return false;
                         }) == nullptr,
           "none installed");
}

void ownChangesAreToldOnce() {
    FakeMixer mixer;
    SystemVolume volume{makeWpctl(mixer.runner())};
    std::vector<VolumeState> told;
    volume.setListener([&](const VolumeState& s) {
        told.push_back(s);
    });
    expect(volume.step(volumeStep).empty() && volume.state() == VolumeState{45, false},
           "a step moves five percent");
    expect(told.size() == 2, "the first read and the change are told");
    told.clear();
    expect(volume.step(-100).empty() && need(volume.state(), "a volume").percent == 0,
           "it stops at silence");
    expect(volume.step(1000).empty() && need(volume.state(), "a volume").percent == 100,
           "and at full");
    told.clear();
    expect(volume.step(10).empty() && told.empty(), "a step that changes nothing tells nothing");
    expect(volume.toggleMute().empty() && need(volume.state(), "a volume").muted &&
               told.size() == 1,
           "mute toggles");
    expect(volume.toggleMute().empty() && !need(volume.state(), "a volume").muted, "and back");
    expect(mixer.state == need(volume.state(), "a volume"), "the mixer holds what the owner says");
}

void foreignChangesAreObservedNotReapplied() {
    FakeMixer mixer;
    SystemVolume volume{makeWpctl(mixer.runner())};
    std::vector<VolumeState> told;
    volume.setListener([&](const VolumeState& s) {
        told.push_back(s);
    });
    volume.refresh();
    told.clear();
    mixer.commands.clear();
    mixer.state.percent = 70;
    const auto start = std::chrono::steady_clock::now();
    volume.poll(start + std::chrono::seconds{5});
    volume.finishPoll();
    expect(told.size() == 1 && told[0].percent == 70, "a change the media keys made is shown");
    for (const std::string& command : mixer.commands) {
        expect(command.find("set-") == std::string::npos, "observing never writes");
    }
    volume.poll(start + std::chrono::seconds{5});
    volume.finishPoll();
    expect(told.size() == 1, "an unchanged level is not shown again");
    volume.step(5);
    mixer.commands.clear();
    volume.poll(start + std::chrono::seconds{9});
    volume.finishPoll();
    expect(told.size() == 2 && told[1].percent == 75,
           "own step shown once; the poll sees no change");
}

void withoutAMixerEverythingIsRefused() {
    SystemVolume volume{nullptr};
    expect(!volume.available() && volume.unavailable().find("sudo") != std::string::npos,
           "the advice names an install command");
    expect(!volume.step(5).empty() && !volume.toggleMute().empty() && !volume.setPercent(3).empty(),
           "every change is refused");
    volume.poll(std::chrono::steady_clock::now());
    expect(!volume.state(), "nothing is read");
}

void readOnlyNeverWrites() {
    FakeMixer mixer;
    SystemVolume volume{makeReadOnly(makeWpctl(mixer.runner()))};
    expect(volume.step(10).empty() && need(volume.state(), "a volume").percent == 50,
           "the change shows");
    expect(volume.toggleMute().empty() && need(volume.state(), "a volume").muted, "mute shows");
    expect(mixer.state == VolumeState({40, false}), "the real mixer is untouched");
    for (const std::string& command : mixer.commands) {
        expect(command.find("set-") == std::string::npos, "no write command ran");
    }
    const auto start = std::chrono::steady_clock::now();
    volume.poll(start + std::chrono::seconds{5});
    volume.finishPoll();
    expect(need(volume.state(), "a volume").percent == 50,
           "polling does not revert the held change");
}

// A real `wpctl status` from a machine with one sink, and one with two.
constexpr const char* wpctlStatus =
    "PipeWire 'pipewire-0' [1.6.9, bhamil@fedora, cookie:3709236677]\n"
    " └─ Clients:\n"
    "        33. uresourced                          [1.6.9, bhamil@fedora, pid:2407]\n"
    "\n"
    "Audio\n"
    " ├─ Devices:\n"
    " │      47. Navi 21/23 HDMI/DP Audio Controller [alsa]\n"
    " │  \n"
    " ├─ Sinks:\n"
    " │      49. Built-in Audio Analog Stereo                         [vol: 1.00]\n"
    " │  *   75. Navi 21/23 HDMI/DP Audio Controller Digital Stereo (HDMI) [SAMSUNG] [vol: 0.35]\n"
    " │  \n"
    " ├─ Sources:\n"
    " │      50. Microphone                                           [vol: 1.00]\n"
    " │  \n"
    " └─ Streams:\n"
    "        87. opensu\n"
    "\n"
    "Video\n"
    " ├─ Sinks:\n"
    " │      99. Not audio\n";

constexpr const char* pactlSinks =
    R"([{"index":1,"name":"alsa_output.analog","description":"Built-in Audio"},)"
    R"({"index":2,"name":"alsa_output.hdmi","description":"HDMI [SAMSUNG]"}])";

void sinkParsing() {
    const std::vector<AudioSink> sinks = parseWpctlSinks(wpctlStatus);
    expect(sinks.size() == 2, "only the Audio section's Sinks block is read");
    expect(sinks[0] == AudioSink{"49", "Built-in Audio Analog Stereo", false}, "a plain sink");
    expect(sinks[1] ==
               AudioSink{"75",
                         "Navi 21/23 HDMI/DP Audio Controller Digital Stereo (HDMI) [SAMSUNG]",
                         true},
           "the starred sink is the default and keeps its bracketed name");
    expect(parseWpctlSinks("nothing").empty(), "noise has no sinks");
    const std::vector<AudioSink> pulse = parsePactlSinks(pactlSinks, "alsa_output.hdmi");
    expect(pulse.size() == 2 && !pulse[0].isDefault && pulse[1].isDefault &&
               pulse[1].name == "HDMI [SAMSUNG]" && pulse[0].id == "alsa_output.analog",
           "pactl sinks");
    expect(parsePactlSinks("not json", "x").empty(), "bad json has no sinks");
}

/// Answers sink commands; each sink has its own volume.
struct SinkMixer {
    std::string current{"75"};
    std::vector<std::string> commands;

    Runner runner() {
        return [this](const std::string& program,
                      const std::vector<std::string>& args) -> std::optional<CommandOutput> {
            std::string line = program;
            for (const std::string& arg : args) {
                line += " " + arg;
            }
            commands.push_back(line);
            if (args.front() == "status") {
                std::string text = wpctlStatus;
                if (current == "49") {
                    text.replace(text.find("*   75"), 6, "    75");
                    text.replace(text.find("│      49"), 9, "│  *   49");
                }
                return CommandOutput{0, text};
            }
            if (args.front() == "set-default") {
                current = args.back();
                return CommandOutput{0, {}};
            }
            if (args.front() == "get-volume") {
                return CommandOutput{0, current == "49" ? "Volume: 1.00\n" : "Volume: 0.35\n"};
            }
            return CommandOutput{0, {}};
        };
    }
};

void switchingTheOutput() {
    SinkMixer mixer;
    const auto wpctl = makeWpctl(mixer.runner());
    expect(wpctl->sinks().size() == 2, "wpctl lists its sinks");
    expect(wpctl->setDefaultSink("49") && mixer.commands.back() == "wpctl set-default 49",
           "wpctl switches with set-default");
    std::vector<std::string> ran;
    const auto pactl =
        makePactl([&](const std::string& program, const std::vector<std::string>& a) {
            std::string line = program;
            for (const std::string& arg : a) {
                line += " " + arg;
            }
            ran.push_back(line);
            if (a.front() == "get-default-sink") {
                return std::optional{CommandOutput{0, "alsa_output.hdmi\n"}};
            }
            return std::optional{CommandOutput{0, a.front() == "--format=json" ? pactlSinks : ""}};
        });
    const std::vector<AudioSink> listed = pactl->sinks();
    expect(listed.size() == 2 && listed[1].isDefault, "pactl lists its sinks and the default");
    expect(pactl->setDefaultSink("alsa_output.analog") &&
               ran.back() == "pactl set-default-sink alsa_output.analog",
           "pactl switches with set-default-sink");

    SystemVolume volume{makeWpctl(mixer.runner())};
    mixer.current = "75";
    std::vector<VolumeState> told;
    volume.refresh();
    volume.setListener([&](const VolumeState& s) {
        told.push_back(s);
    });
    expect(volume.setDefaultSink("49").empty(), "the switch is accepted");
    expect(told.size() == 1 && told[0].percent == 100,
           "the new output's own volume is read and told");
    expect(SystemVolume{nullptr}.sinks().empty() &&
               !SystemVolume{nullptr}.setDefaultSink("1").empty(),
           "no mixer, no outputs and a refusal");
}

void readOnlyOutputs() {
    SinkMixer mixer;
    SystemVolume volume{makeReadOnly(makeWpctl(mixer.runner()))};
    expect(volume.setDefaultSink("49").empty(), "a held switch is accepted");
    const std::vector<AudioSink> sinks = volume.sinks();
    expect(sinks.size() == 2 && sinks[0].isDefault && !sinks[1].isDefault,
           "the held default shows in the list");
    expect(!volume.setDefaultSink("nope").empty(), "an unknown sink is refused");
    expect(mixer.current == "75", "the real default is untouched");
    for (const std::string& command : mixer.commands) {
        expect(command.find("set-") == std::string::npos, "no write command ran");
    }
}

} // namespace

int main() {
    sinkParsing();
    switchingTheOutput();
    readOnlyOutputs();
    parsing();
    backendsSpeakTheirMixer();
    detectionPrefersWpctl();
    ownChangesAreToldOnce();
    foreignChangesAreObservedNotReapplied();
    withoutAMixerEverythingIsRefused();
    readOnlyNeverWrites();
    std::printf("volume: all checks passed\n");
    return 0;
}
