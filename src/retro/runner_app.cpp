#include "runner_app.hpp"

#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include <SDL3/SDL_scancode.h>

#include "audio_output.hpp"
#include "audio_sink.hpp"
#include "control_server.hpp"
#include "core.hpp"
#include "core_environment.hpp"
#include "fileio/atomic_write.hpp"
#include "gamepad/pads.hpp"
#include "joypad.hpp"
#include "lucent/log.h"
#include "png_encoder.hpp"
#include "presenter.hpp"
#include "save_memory.hpp"
#include "save_state.hpp"
#include "video_frame.hpp"

namespace opensu::retro {
namespace {

using Clock = std::chrono::steady_clock;

constexpr int outputRate = 48000;
/// About 40 ms of sound buffered ahead of the device.
constexpr std::size_t audioTarget = outputRate / 25;
constexpr auto saveInterval = std::chrono::seconds{1};
/// Further behind than this, the clock starts over instead of running frames back to back.
constexpr int maxLateFrames = 4;

Clock::duration frameInterval(double fps) {
    const double rate = fps > 0.0 ? fps : 60.0;
    return std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>{1.0 / rate});
}

float aspectOf(const retro_game_geometry& geometry) {
    return geometry.aspect_ratio > 0.0F ? geometry.aspect_ratio : 0.0F;
}

void makeDirectory(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        lucent::warn("retro", "cannot make {}: {}", directory.string(), error.message());
    }
}

/// The pads read without grabbing, the shell's virtual pads among them; none when unreadable.
std::unique_ptr<gamepad::Pads> openPads() {
    try {
        return std::make_unique<gamepad::Pads>("/dev/input", gamepad::OwnVirtualPads::Read);
    } catch (const std::system_error& failure) {
        lucent::warn("retro", "no pads: {}", failure.what());
        return nullptr;
    }
}

std::unique_ptr<AudioOutput> openAudio(AudioSink& sink, bool hidden) {
    if (hidden) {
        return std::make_unique<ClockOutput>(sink);
    }
    try {
        return std::make_unique<DeviceOutput>(sink);
    } catch (const std::runtime_error& failure) {
        lucent::warn("retro", "playing without sound: {}", failure.what());
        return std::make_unique<ClockOutput>(sink);
    }
}

ControlAnswer answerOf(const StateOutcome& outcome) {
    switch (outcome.result) {
    case StateResult::Done:
        return {};
    case StateResult::Refused:
        return {409, outcome.reason};
    case StateResult::Missing:
        return {404, outcome.reason};
    case StateResult::FileFailed:
        break;
    }
    return {500, outcome.reason};
}

ControlAnswer perform(ControlCommand command, Core& core, SaveState& state) {
    ControlAnswer answer;
    switch (command) {
    case ControlCommand::SaveState:
        answer = answerOf(state.save(core));
        break;
    case ControlCommand::LoadState:
        answer = answerOf(state.load(core));
        break;
    case ControlCommand::Reset:
        core.reset();
        lucent::info("retro", "game reset");
        break;
    }
    answer.hasState = state.exists();
    return answer;
}

/// The control channel when the shell asked for one and it listens; none otherwise.
std::unique_ptr<ControlServer> openControl(const RunnerOptions& options, const Core& core) {
    if (options.control.empty()) {
        return nullptr;
    }
    auto control =
        std::make_unique<ControlServer>(options.control, core.name(), options.rom.string());
    if (!control->start()) {
        return nullptr;
    }
    return control;
}

bool writeFramePng(Presenter& presenter, const std::filesystem::path& file) {
    Readback image;
    try {
        image = presenter.readBack();
    } catch (const VulkanError& failure) {
        lucent::error("retro", "{}", failure.what());
        return false;
    }
    if (image.rgba.empty()) {
        lucent::error("retro", "no frame was shown to write to {}", file.string());
        return false;
    }
    const std::string png = encodePng(image.rgba, image.width, image.height);
    std::string error;
    if (png.empty() || !fileio::writeWhole(file, png, error)) {
        lucent::error("retro", "cannot write {}: {}", file.string(), error);
        return false;
    }
    lucent::info("retro", "wrote {}x{} frame to {}", image.width, image.height, file.string());
    return true;
}

} // namespace

RunnerApp::RunnerApp(RunnerOptions options) : options_{std::move(options)} {
}

RunnerExit RunnerApp::run() {
    makeDirectory(options_.saves);
    makeDirectory(options_.system);
    CoreEnvironment environment{options_.system, options_.saves};
    VideoFrame video;
    AudioSink sink{outputRate, audioTarget};
    Joypad joypad;
    std::unique_ptr<Core> core;
    try {
        core = std::make_unique<Core>(options_.core, CoreHost{environment, video, sink, joypad});
        core->loadGame(options_.rom);
    } catch (const CoreError& failure) {
        lucent::error("retro", "{}", failure.what());
        if (!options_.control.empty()) {
            ControlServer::reportLoadFailure(options_.control, failure.what());
        }
        return RunnerExit::LoadFailed;
    }
    retro_game_geometry geometry = core->avInfo().geometry;
    Clock::duration interval = frameInterval(core->avInfo().timing.fps);
    sink.setCoreRate(core->avInfo().timing.sample_rate);

    SaveMemory saves{options_.saves / (options_.rom.stem().string() + ".srm")};
    saves.load(core->saveRam());
    SaveState state{options_.saves / (options_.rom.stem().string() + ".state")};

    std::unique_ptr<Presenter> presenter;
    try {
        presenter = std::make_unique<Presenter>(PresenterOptions{
            .hidden = options_.hidden,
            .fullscreen = options_.fullscreen,
            .width = options_.width,
            .height = options_.height,
            .validation = options_.validation,
        });
    } catch (const std::exception& failure) {
        lucent::error("retro", "{}", failure.what());
        return RunnerExit::PresenterFailed;
    }
    const std::unique_ptr<AudioOutput> audio = openAudio(sink, options_.hidden);
    const std::unique_ptr<gamepad::Pads> pads = options_.hidden ? nullptr : openPads();
    const std::unique_ptr<ControlServer> control = openControl(options_, *core);

    RunnerExit exit = RunnerExit::Quit;
    std::uint64_t frame = 0;
    Clock::time_point next = Clock::now();
    Clock::time_point nextSave = next + saveInterval;
    while (!options_.frames || frame < *options_.frames) {
        const WindowInput input = presenter->poll();
        if (input.quit) {
            break;
        }
        if (!options_.hidden) {
            bool escape = false;
            for (const WindowInput::Key& key : input.keys) {
                escape = escape || (key.down && key.scancode == SDL_SCANCODE_ESCAPE);
                joypad.key(key.scancode, key.down);
            }
            if (escape) {
                break;
            }
        }
        if (pads) {
            for (const gamepad::Event& event : pads->takeEvents()) {
                joypad.apply(event);
            }
        }

        if (control) {
            control->serve([&core, &state](ControlCommand command) {
                return perform(command, *core, state);
            });
        }
        core->run();
        ++frame;
        if (control) {
            control->publish(frame, state.exists());
        }
        if (const auto timing = environment.takeTiming()) {
            interval = frameInterval(timing->fps);
            sink.setCoreRate(timing->sample_rate);
        }
        if (const auto changed = environment.takeGeometry()) {
            geometry = *changed;
        }
        const Clock::time_point now = Clock::now();
        audio->service(now);
        try {
            presenter->present(
                FrameImage{video.rgba(), video.width(), video.height(), aspectOf(geometry)},
                video.takeFresh());
        } catch (const VulkanError& failure) {
            lucent::error("retro", "{}", failure.what());
            exit = RunnerExit::PresenterFailed;
            break;
        }
        if (now >= nextSave) {
            saves.flush(core->saveRam());
            nextSave = now + saveInterval;
        }

        next += interval;
        if (Clock::now() - next > interval * maxLateFrames) {
            next = Clock::now();
        } else {
            std::this_thread::sleep_until(next);
        }
    }
    const auto millisecondsOf = [&sink](std::size_t frames) {
        return frames * 1000 / static_cast<std::size_t>(sink.outputRate());
    };
    lucent::info("retro", "ran {} frames; sound: {} ms played as silence, {} ms dropped", frame,
                 millisecondsOf(sink.underrunFrames()), millisecondsOf(sink.overrunFrames()));
    saves.flush(core->saveRam());

    if (exit == RunnerExit::Quit && options_.framePng &&
        !writeFramePng(*presenter, *options_.framePng)) {
        exit = RunnerExit::PresenterFailed;
    }
    if (exit == RunnerExit::Quit && presenter->validationErrors() > 0) {
        lucent::error("retro", "Vulkan validation reported {} errors",
                      presenter->validationErrors());
        exit = RunnerExit::ValidationErrors;
    }
    return exit;
}

} // namespace opensu::retro
