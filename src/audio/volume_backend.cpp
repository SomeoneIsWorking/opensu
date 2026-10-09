#include "volume_backend.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>

#include <nlohmann/json.hpp>

namespace opensu::audio {
namespace {

constexpr const char* wpctlSink = "@DEFAULT_AUDIO_SINK@";
constexpr const char* pactlSink = "@DEFAULT_SINK@";

int clampPercent(int percent) noexcept {
    return std::clamp(percent, 0, 100);
}

std::string percentArgument(int percent) {
    return std::to_string(clampPercent(percent)) + "%";
}

class Wpctl final : public VolumeBackend {
  public:
    explicit Wpctl(Runner run) : run_{std::move(run)} {
    }
    [[nodiscard]] std::string_view name() const override {
        return "wpctl";
    }
    std::optional<VolumeState> read() override {
        const std::optional<CommandOutput> out = run_("wpctl", {"get-volume", wpctlSink});
        return out && out->status == 0 ? parseWpctl(out->output) : std::nullopt;
    }
    bool setPercent(int percent) override {
        return succeeded(run_("wpctl", {"set-volume", wpctlSink, percentArgument(percent)}));
    }
    bool setMuted(bool muted) override {
        return succeeded(run_("wpctl", {"set-mute", wpctlSink, muted ? "1" : "0"}));
    }
    std::vector<AudioSink> sinks() override {
        const std::optional<CommandOutput> out = run_("wpctl", {"status"});
        return out && out->status == 0 ? parseWpctlSinks(out->output) : std::vector<AudioSink>{};
    }
    bool setDefaultSink(const std::string& id) override {
        return succeeded(run_("wpctl", {"set-default", id}));
    }

  private:
    static bool succeeded(const std::optional<CommandOutput>& out) {
        return out && out->status == 0;
    }
    Runner run_;
};

class Pactl final : public VolumeBackend {
  public:
    explicit Pactl(Runner run) : run_{std::move(run)} {
    }
    [[nodiscard]] std::string_view name() const override {
        return "pactl";
    }
    std::optional<VolumeState> read() override {
        const std::optional<CommandOutput> volume = run_("pactl", {"get-sink-volume", pactlSink});
        const std::optional<CommandOutput> mute = run_("pactl", {"get-sink-mute", pactlSink});
        if (!volume || volume->status != 0 || !mute || mute->status != 0) {
            return std::nullopt;
        }
        return parsePactl(volume->output, mute->output);
    }
    bool setPercent(int percent) override {
        return succeeded(run_("pactl", {"set-sink-volume", pactlSink, percentArgument(percent)}));
    }
    bool setMuted(bool muted) override {
        return succeeded(run_("pactl", {"set-sink-mute", pactlSink, muted ? "1" : "0"}));
    }
    std::vector<AudioSink> sinks() override {
        const std::optional<CommandOutput> list = run_("pactl", {"--format=json", "list", "sinks"});
        const std::optional<CommandOutput> current = run_("pactl", {"get-default-sink"});
        if (!list || list->status != 0) {
            return {};
        }
        std::string defaultName = current && current->status == 0 ? current->output : "";
        while (!defaultName.empty() && (defaultName.back() == '\n' || defaultName.back() == ' ')) {
            defaultName.pop_back();
        }
        return parsePactlSinks(list->output, defaultName);
    }
    bool setDefaultSink(const std::string& id) override {
        return succeeded(run_("pactl", {"set-default-sink", id}));
    }

  private:
    static bool succeeded(const std::optional<CommandOutput>& out) {
        return out && out->status == 0;
    }
    Runner run_;
};

class ReadOnly final : public VolumeBackend {
  public:
    explicit ReadOnly(std::unique_ptr<VolumeBackend> inner) : inner_{std::move(inner)} {
    }
    [[nodiscard]] std::string_view name() const override {
        return inner_->name();
    }
    std::optional<VolumeState> read() override {
        if (!held_) {
            held_ = inner_->read();
        }
        return held_;
    }
    bool setPercent(int percent) override {
        const std::optional<VolumeState> now = read();
        if (!now) {
            return false;
        }
        held_ = VolumeState{clampPercent(percent), now->muted};
        return true;
    }
    bool setMuted(bool muted) override {
        const std::optional<VolumeState> now = read();
        if (!now) {
            return false;
        }
        held_ = VolumeState{now->percent, muted};
        return true;
    }
    std::vector<AudioSink> sinks() override {
        std::vector<AudioSink> all = inner_->sinks();
        if (!chosen_) {
            return all;
        }
        for (AudioSink& sink : all) {
            sink.isDefault = sink.id == *chosen_;
        }
        return all;
    }
    bool setDefaultSink(const std::string& id) override {
        const std::vector<AudioSink> all = inner_->sinks();
        if (std::ranges::none_of(all, [&id](const AudioSink& sink) {
                return sink.id == id;
            })) {
            return false;
        }
        chosen_ = id;
        return true;
    }

  private:
    std::unique_ptr<VolumeBackend> inner_;
    std::optional<VolumeState> held_;
    std::optional<std::string> chosen_;
};

/// The number that follows `marker` in `text`, or nothing.
std::optional<double> numberAfter(std::string_view text, std::string_view marker) {
    const std::size_t at = text.find(marker);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    std::string_view rest = text.substr(at + marker.size());
    while (!rest.empty() && rest.front() == ' ') {
        rest.remove_prefix(1);
    }
    double value = 0.0;
    const auto [end, error] = std::from_chars(rest.data(), rest.data() + rest.size(), value);
    if (error != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::optional<VolumeState> parseWpctl(std::string_view output) {
    const std::optional<double> level = numberAfter(output, "Volume:");
    if (!level) {
        return std::nullopt;
    }
    return VolumeState{clampPercent(static_cast<int>(std::lround(*level * 100.0))),
                       output.find("[MUTED]") != std::string_view::npos};
}

std::optional<VolumeState> parsePactl(std::string_view volume, std::string_view mute) {
    // "front-left: 26214 /  40% / -23.88 dB": the first percentage is the left channel's.
    const std::size_t percent = volume.find('%');
    if (percent == std::string_view::npos) {
        return std::nullopt;
    }
    std::size_t start = percent;
    while (start > 0 && volume[start - 1] >= '0' && volume[start - 1] <= '9') {
        --start;
    }
    int level = 0;
    const auto [end, error] =
        std::from_chars(volume.data() + start, volume.data() + percent, level);
    if (error != std::errc{} || end != volume.data() + percent) {
        return std::nullopt;
    }
    const std::size_t verdict = mute.find("Mute:");
    if (verdict == std::string_view::npos) {
        return std::nullopt;
    }
    return VolumeState{clampPercent(level),
                       mute.substr(verdict).find("yes") != std::string_view::npos};
}

std::vector<AudioSink> parseWpctlSinks(std::string_view status) {
    std::vector<AudioSink> sinks;
    bool inAudio = false;
    bool inSinks = false;
    std::size_t at = 0;
    while (at < status.size()) {
        std::size_t end = status.find('\n', at);
        if (end == std::string_view::npos) {
            end = status.size();
        }
        std::string_view line = status.substr(at, end - at);
        at = end + 1;
        // Top-level headings start in column 0 ("Audio", "Video", "Settings").
        if (!line.empty() && line.front() != ' ') {
            inAudio = line == "Audio";
            inSinks = false;
            continue;
        }
        if (!inAudio) {
            continue;
        }
        // Section headings are drawn after tree glyphs: "├─ Sinks:".
        if (const std::size_t colon = line.find(':');
            colon != std::string_view::npos && line.find("─ ") != std::string_view::npos) {
            inSinks = line.substr(0, colon).ends_with("Sinks");
            continue;
        }
        if (!inSinks) {
            continue;
        }
        const std::size_t dot = line.find(". ");
        if (dot == std::string_view::npos) {
            continue;
        }
        std::size_t start = dot;
        while (start > 0 && line[start - 1] >= '0' && line[start - 1] <= '9') {
            --start;
        }
        if (start == dot) {
            continue;
        }
        AudioSink sink;
        sink.id = std::string{line.substr(start, dot - start)};
        sink.isDefault = line.substr(0, start).find('*') != std::string_view::npos;
        std::string_view name = line.substr(dot + 2);
        if (const std::size_t volume = name.rfind("[vol:"); volume != std::string_view::npos) {
            name = name.substr(0, volume);
        }
        while (!name.empty() && name.back() == ' ') {
            name.remove_suffix(1);
        }
        sink.name = std::string{name};
        sinks.push_back(std::move(sink));
    }
    return sinks;
}

std::vector<AudioSink> parsePactlSinks(std::string_view json, std::string_view defaultName) {
    const nlohmann::json parsed = nlohmann::json::parse(json, nullptr, false);
    std::vector<AudioSink> sinks;
    if (!parsed.is_array()) {
        return sinks;
    }
    for (const nlohmann::json& entry : parsed) {
        if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string()) {
            continue;
        }
        AudioSink sink;
        sink.id = entry["name"].get<std::string>();
        sink.name = entry.value("description", sink.id);
        sink.isDefault = sink.id == defaultName;
        sinks.push_back(std::move(sink));
    }
    return sinks;
}

std::unique_ptr<VolumeBackend> makeWpctl(Runner run) {
    return std::make_unique<Wpctl>(std::move(run));
}

std::unique_ptr<VolumeBackend> makePactl(Runner run) {
    return std::make_unique<Pactl>(std::move(run));
}

std::unique_ptr<VolumeBackend>
detectBackend(Runner run, const std::function<bool(const std::string& program)>& exists) {
    if (exists("wpctl")) {
        return makeWpctl(std::move(run));
    }
    if (exists("pactl")) {
        return makePactl(std::move(run));
    }
    return nullptr;
}

std::unique_ptr<VolumeBackend> makeReadOnly(std::unique_ptr<VolumeBackend> inner) {
    return std::make_unique<ReadOnly>(std::move(inner));
}

std::string missingBackendAdvice() {
    return "no volume control found; install wpctl: sudo dnf install wireplumber, "
           "sudo apt install wireplumber or sudo pacman -S wireplumber";
}

} // namespace opensu::audio
