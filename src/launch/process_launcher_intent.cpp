#include <SDL3/SDL_error.h>

#include "process_launcher.hpp"

#include "activity_call.hpp"

namespace opensu::launch {

/// The launch lives in the shell Activity's `Launches`: the runner's `:retro` process, or another
/// app opened by an intent. Nothing of it is held here.
struct ProcessLauncher::Group {};

namespace {

using android::ActivityCall;

/// Whether the Activity's last launch still runs; false when the Activity cannot be asked.
bool launchRunning() {
    const ActivityCall call;
    if (!call.ready()) {
        return false;
    }
    const jboolean running =
        call.env()->CallBooleanMethod(call.activity(), call.method("launchRunning", "()Z"));
    return call.takeException().empty() && running == JNI_TRUE;
}

} // namespace

ProcessLauncher::ProcessLauncher() = default;

// Launches.start says when an app is missing.
bool ProcessLauncher::startable(const std::string& program,
                                const std::vector<std::filesystem::path>& /*executablePath*/) {
    return !program.empty();
}

bool ProcessLauncher::keepsOutput() {
    return true;
}

ProcessLauncher::~ProcessLauncher() = default;

// The working directory and the output file have no counterpart for an Activity: the runner's
// output goes to logcat like the shell's.
bool ProcessLauncher::start(const std::string& /*group*/, const std::string& program,
                            const std::vector<std::string>& args, std::string& failure,
                            const std::vector<std::string>& environment,
                            const std::filesystem::path& /*workingDirectory*/,
                            const std::filesystem::path& /*output*/) {
    const std::lock_guard lock{mutex_};
    const ActivityCall call;
    if (!call.ready()) {
        failure = "the shell's Activity is unavailable: " + std::string{SDL_GetError()};
        return false;
    }
    JNIEnv* env = call.env();
    auto* refused = static_cast<jstring>(env->CallObjectMethod(
        call.activity(),
        call.method("launch",
                    "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/String;"),
        env->NewStringUTF(program.c_str()), call.strings(args), call.strings(environment)));
    if (std::string thrown = call.takeException(); !thrown.empty()) {
        failure = std::move(thrown);
        return false;
    }
    if (refused != nullptr) {
        failure = call.string(refused);
        return false;
    }
    group_ = std::make_unique<Group>();
    exitStatus_ = -1;
    return true;
}

bool ProcessLauncher::running() {
    const std::lock_guard lock{mutex_};
    if (group_ == nullptr) {
        return false;
    }
    if (launchRunning()) {
        return true;
    }
    if (exitStatus_ < 0) {
        exitStatus_ = 0;
    }
    return false;
}

bool ProcessLauncher::startedRunning() {
    return running();
}

void ProcessLauncher::stop() {
    const std::lock_guard lock{mutex_};
    if (group_ == nullptr) {
        return;
    }
    const bool wasRunning = launchRunning();
    const ActivityCall call;
    if (call.ready()) {
        call.env()->CallVoidMethod(call.activity(), call.method("stopLaunch", "()V"));
        (void)call.takeException();
    }
    if (exitStatus_ < 0) {
        // Ended by SIGKILL, as the scope's stop reports a killed game on Linux.
        exitStatus_ = wasRunning ? 128 + 9 : 0;
    }
}

int ProcessLauncher::exitStatus() const {
    const std::lock_guard lock{mutex_};
    return exitStatus_;
}

} // namespace opensu::launch
