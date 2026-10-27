#include "runner_app.hpp"

#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include <SDL3/SDL_scancode.h>
#include <nlohmann/json.hpp>

#include "audio_output.hpp"
#include "audio_sink.hpp"
#include "control_server.hpp"
#include "core.hpp"
#include "core_environment.hpp"
#include "fileio/atomic_write.hpp"
#include "gamepad/pad_source.hpp"
#include "gpu2d/image/png_encoder.hpp"
#include "hardware_render.hpp"
#include "joypad.hpp"
#include "librashader_library.hpp"
#include "lucent/log.h"
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
std::unique_ptr<gamepad::PadSource> openPads() {
    try {
        return gamepad::openPadSource({.own = gamepad::OwnVirtualPads::Read});
    } catch (const std::runtime_error& failure) {
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

/// The last image `presenter` showed as PNG; empty with `error` when there is none or it fails.
std::string framePng(Presenter& presenter, std::string& error) {
    Readback frame;
    try {
        frame = presenter.readBack();
    } catch (const VulkanError& failure) {
        error = failure.what();
        return {};
    }
    if (frame.rgba.empty()) {
        error = "no frame was shown";
        return {};
    }
    return gpu2d::image::encodePng(frame.rgba, frame.width, frame.height, error);
}

/// What the frame loop runs a control command on.
struct Commanded {
    Core& core;
    SaveState& state;
    Presenter& presenter;
    CoreOptions& options;
    bool hidden;
};

std::string optionsJson(const CoreOptions& options) {
    nlohmann::json list = nlohmann::json::array();
    for (const CoreOptions::Option& option : options.options()) {
        list.push_back(nlohmann::json{{"key", option.key},
                                      {"label", option.label},
                                      {"info", option.info},
                                      {"values", option.values},
                                      {"labels", option.valueLabels},
                                      {"default", option.fallback},
                                      {"value", option.current}});
    }
    return list.dump();
}

ControlAnswer changeVideo(Presenter& presenter, const settings::VideoSettings& video) {
    try {
        presenter.change(video);
    } catch (const ShaderError& failure) {
        lucent::error("retro", "{}", failure.what());
        return {409, failure.what()};
    }
    lucent::info("retro", "video now {}", settings::videoJson(video));
    return {};
}

ControlAnswer perform(const ControlRequest& request, const Commanded& on) {
    ControlAnswer answer;
    switch (request.command) {
    case ControlCommand::SaveState:
        answer = answerOf(on.state.save(on.core));
        break;
    case ControlCommand::LoadState:
        answer = answerOf(on.state.load(on.core));
        break;
    case ControlCommand::Reset:
        on.core.reset();
        lucent::info("retro", "game reset");
        break;
    case ControlCommand::Video:
        answer = changeVideo(on.presenter, request.video);
        break;
    case ControlCommand::Options:
        answer.json = optionsJson(on.options);
        break;
    case ControlCommand::Option:
        if (!on.options.set(request.key, request.value)) {
            answer = {409, "no such core option or value"};
            break;
        }
        answer.json = optionsJson(on.options);
        break;
    case ControlCommand::Frame: {
        if (!on.hidden) {
            answer = {409, "the frame is read back in hidden runs only"};
            break;
        }
        std::string error;
        answer.png = framePng(on.presenter, error);
        if (answer.png.empty()) {
            answer = {500, error};
        }
        break;
    }
    }
    answer.hasState = on.state.exists();
    return answer;
}

/// The control channel's pad holds `held` alone: what it held before is let go.
void holdControlPad(Joypad& joypad, const ControlInput& held) {
    gamepad::Event event;
    event.source = "control";
    event.kind = gamepad::Event::Kind::Disconnected;
    joypad.apply(event);
    event.kind = gamepad::Event::Kind::Button;
    event.pressed = true;
    for (const gamepad::Button button : held.buttons) {
        event.button = button;
        joypad.apply(event);
    }
    joypad.sticks(event.source, held.sticks);
    lucent::info("retro", "control pad holds {} buttons, left stick {},{}, right stick {},{}",
                 held.buttons.size(), held.sticks.leftX, held.sticks.leftY, held.sticks.rightX,
                 held.sticks.rightY);
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
    std::string error;
    const std::string png = framePng(presenter, error);
    if (png.empty() || !fileio::writeWhole(file, png, error)) {
        lucent::error("retro", "cannot write {}: {}", file.string(), error);
        return false;
    }
    lucent::info("retro", "wrote the last frame to {}", file.string());
    return true;
}

} // namespace

RunnerApp::RunnerApp(RunnerOptions options) : options_{std::move(options)} {
}

RunnerExit RunnerApp::run() {
    makeDirectory(options_.saves);
    makeDirectory(options_.system);
    CoreEnvironment environment{options_.system, options_.saves, options_.coreOptions};
    HardwareRender& hardware = environment.hardware();
    VideoFrame video;
    AudioSink sink{outputRate, audioTarget};
    Joypad joypad;
    std::unique_ptr<Core> core;
    try {
        core = std::make_unique<Core>(options_.core, CoreHost{environment, video, sink, joypad});
        core->loadGame(options_.rom);
    } catch (const CoreError& failure) {
        std::string reason = failure.what();
        if (!hardware.refusal().empty()) {
            reason += ": " + hardware.refusal();
        }
        lucent::error("retro", "{}", reason);
        if (!options_.control.empty()) {
            ControlServer::reportLoadFailure(options_.control, reason);
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
            .negotiation = hardware.requested() ? hardware.negotiation() : nullptr,
            .video = options_.video,
            .librashader = options_.librashader,
        });
    } catch (const std::exception& failure) {
        lucent::error("retro", "{}", failure.what());
        // A refused shader preset is the player's to read; the shell shows it.
        if (!options_.control.empty()) {
            ControlServer::reportLoadFailure(options_.control, failure.what());
        }
        return RunnerExit::PresenterFailed;
    }
    hardware.attach(*presenter);
    const std::unique_ptr<AudioOutput> audio = openAudio(sink, options_.hidden);
    const std::unique_ptr<gamepad::PadSource> pads = options_.hidden ? nullptr : openPads();
    const std::unique_ptr<ControlServer> control = openControl(options_, *core);

    RunnerExit exit = RunnerExit::Quit;
    std::uint64_t frame = 0;
    /// Whether the last frame came as the core's Vulkan image rather than pixels.
    bool showingCoreImage = false;
    Clock::time_point next = Clock::now();
    Clock::time_point nextSave = next + saveInterval;
    while (!options_.frames || frame < *options_.frames) {
        const WindowInput input = presenter->poll();
        if (input.quit) {
            break;
        }
        if (!options_.hidden) {
            for (const WindowInput::Key& key : input.keys) {
                joypad.key(key.scancode, key.down);
            }
        }
        if (pads) {
            for (const gamepad::Event& event : pads->takeEvents()) {
                joypad.apply(event);
            }
            for (const auto& [source, sticks] : pads->sticks()) {
                joypad.sticks(source, sticks);
            }
        }

        if (control) {
            if (const std::optional<ControlInput> held = control->takeInput()) {
                holdControlPad(joypad, *held);
            }
            control->serve([&](const ControlRequest& request) {
                return perform(request, Commanded{*core, state, *presenter, environment.options(),
                                                  options_.hidden});
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
        const bool coreImageFresh = hardware.takeFresh();
        const bool pixelsFresh = video.takeFresh();
        if (coreImageFresh || pixelsFresh) {
            showingCoreImage = coreImageFresh;
        }
        const float aspect = aspectOf(geometry);
        const std::optional<FrameImage> coreImage =
            showingCoreImage ? hardware.frame(aspect) : std::nullopt;
        try {
            presenter->present(coreImage.value_or(FrameImage{
                                   video.rgba(), video.width(), video.height(), aspect, {}}),
                               coreImageFresh || pixelsFresh, hardware.takeSync());
        } catch (const VulkanError& failure) {
            lucent::error("retro", "{}", failure.what());
            exit = RunnerExit::PresenterFailed;
            break;
        } catch (const ShaderError& failure) {
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
    // A GPU core's context goes before its game, and the device after both.
    hardware.detach();
    core->unloadGame();
    if (exit == RunnerExit::Quit && presenter->validationErrors() > 0) {
        lucent::error("retro", "Vulkan validation reported {} errors",
                      presenter->validationErrors());
        exit = RunnerExit::ValidationErrors;
    }
    presenter.reset();
    return exit;
}

} // namespace opensu::retro
