#include "audio_manager_volume.hpp"

#include "activity_call.hpp"

namespace opensu::audio {

namespace {

using android::ActivityCall;

class AudioManagerVolume final : public VolumeBackend {
  public:
    [[nodiscard]] std::string_view name() const override {
        return "AudioManager";
    }

    [[nodiscard]] std::optional<VolumeState> read() override {
        const ActivityCall call;
        if (!call.ready()) {
            return std::nullopt;
        }
        auto* values = static_cast<jintArray>(
            call.env()->CallObjectMethod(call.activity(), call.method("musicVolume", "()[I")));
        if (!call.takeException().empty() || values == nullptr ||
            call.env()->GetArrayLength(values) != 2) {
            return std::nullopt;
        }
        jint state[2]{};
        call.env()->GetIntArrayRegion(values, 0, 2, state);
        return VolumeState{state[0], state[1] != 0};
    }

    bool setPercent(int percent) override {
        const ActivityCall call;
        return call.ready() &&
               call.env()->CallBooleanMethod(call.activity(), call.method("setMusicVolume", "(I)Z"),
                                             static_cast<jint>(percent)) == JNI_TRUE &&
               call.takeException().empty();
    }

    bool setMuted(bool muted) override {
        const ActivityCall call;
        return call.ready() &&
               call.env()->CallBooleanMethod(call.activity(), call.method("setMusicMuted", "(Z)Z"),
                                             muted ? JNI_TRUE : JNI_FALSE) == JNI_TRUE &&
               call.takeException().empty();
    }

    [[nodiscard]] std::vector<AudioSink> sinks() override {
        return {};
    }

    bool setDefaultSink(const std::string& /*id*/) override {
        return false;
    }
};

} // namespace

std::unique_ptr<VolumeBackend> makeAudioManager() {
    return std::make_unique<AudioManagerVolume>();
}

} // namespace opensu::audio
