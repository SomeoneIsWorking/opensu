#include "activity_call.hpp"
#include "game_overlay.hpp"
#include "lucent/log.h"

namespace opensu::app {
namespace {

using android::ActivityCall;

/// The shell's Activity in front of the runner's: the runner pauses its game and brings the shell
/// forward when Guide, Back or its pause button is pressed, which reads here as Guide. Hiding the
/// menu sends the game back to the front; when the game ends the shell comes back in front of
/// whatever Android shows next.
class ActivityTaskOverlay final : public GameOverlay {
  public:
    void enter() override {
    }
    void show(display::OverlayMode mode) override {
        if (mode == display::OverlayMode::Interactive) {
            return;
        }
        const ActivityCall call;
        if (!call.ready()) {
            lucent::warn("shell", "the game cannot be brought back: no Activity");
            return;
        }
        const jboolean resumed =
            call.env()->CallBooleanMethod(call.activity(), call.method("resumeGame", "()Z"));
        if (const std::string failure = call.takeException(); !failure.empty()) {
            lucent::warn("shell", "the game cannot be brought back: {}", failure);
            return;
        }
        lucent::info("shell", "game brought back to the front: {}", resumed == JNI_TRUE);
    }
    void leave() override {
        const ActivityCall call;
        if (!call.ready()) {
            lucent::warn("shell", "the shell cannot come back to the front: no Activity");
            return;
        }
        call.env()->CallVoidMethod(call.activity(), call.method("showShell", "()V"));
        if (const std::string failure = call.takeException(); !failure.empty()) {
            lucent::warn("shell", "the shell cannot come back to the front: {}", failure);
        }
    }
    std::vector<input::Action> gameKeys(const input::Shortcuts& /*shortcuts*/) override {
        const ActivityCall call;
        if (!call.ready()) {
            return {};
        }
        const jboolean requested =
            call.env()->CallBooleanMethod(call.activity(), call.method("takeMenuRequest", "()Z"));
        if (!call.takeException().empty() || requested != JNI_TRUE) {
            return {};
        }
        return {input::Action::Guide};
    }
};

} // namespace

std::unique_ptr<GameOverlay> openGameOverlay(ShellWindow& /*window*/) {
    return std::make_unique<ActivityTaskOverlay>();
}

} // namespace opensu::app
