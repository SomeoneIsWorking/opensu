#include "runner_app.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include <SDL3/SDL_hints.h>
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
#include "shell_handoff.hpp"
#include "touch_layer.hpp"
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
    joypad.hold("control", held.buttons);
    joypad.sticks("control", held.sticks);
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

/// `buttons` by their prompts, "nothing" for none.
std::string buttonList(const std::vector<gamepad::Button>& buttons) {
    std::string list;
    for (const gamepad::Button button : buttons) {
        list += (list.empty() ? "" : " ") + std::string{gamepad::prompt(button)};
    }
    return list.empty() ? "nothing" : list;
}

/// Asks for the shell's menu, the game waiting while it is up.
void openShellMenu(const char* by) {
    lucent::info("retro", "{} asks for the shell's menu", by);
    if (!shell_handoff::openMenu()) {
        lucent::warn("retro", "the shell's menu did not open");
    }
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
    try {
        saves.load(core->saveRam());
    } catch (const SaveError& failure) {
        lucent::error("retro", "{}", failure.what());
        if (!options_.control.empty()) {
            ControlServer::reportLoadFailure(options_.control, failure.what());
        }
        return RunnerExit::LoadFailed;
    }
    SaveState state{options_.saves / (options_.rom.stem().string() + ".state")};

    const bool handoff = shell_handoff::active() && !options_.hidden;
    if (handoff) {
        // Back asks for the shell's menu instead of closing the Activity, and the loop keeps
        // serving the shell's save and load requests while the shell is in front.
        SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
        SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
    }
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

    std::unique_ptr<TouchLayer> touch =
        handoff ? std::make_unique<TouchLayer>(options_.touchArt) : nullptr;
    WindowSurface laidOut{};
    /// Fingers lifted in the frame they went down, let go the next.
    std::vector<Finger> liftNext;
    bool padConnected = pads && !pads->connected().empty();
    /// In the background: the game waits.
    bool waiting = false;

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
        if (handoff && input.shown) {
            waiting = !*input.shown;
            lucent::info("retro", "the game {}", waiting ? "waits in the background" : "goes on");
        }
        if (!options_.hidden) {
            for (const WindowInput::Key& key : input.keys) {
                if (handoff && key.scancode == SDL_SCANCODE_AC_BACK) {
                    if (key.down) {
                        openShellMenu("Back");
                    }
                    continue;
                }
                joypad.key(key.scancode, key.down);
            }
        }
        if (pads) {
            for (const gamepad::Event& event : pads->takeEvents()) {
                if (event.kind == gamepad::Event::Kind::Connected ||
                    event.kind == gamepad::Event::Kind::Disconnected) {
                    padConnected = !pads->connected().empty();
                }
                if (handoff && event.kind == gamepad::Event::Kind::Button &&
                    event.button == gamepad::Button::Guide && event.pressed) {
                    openShellMenu("a pad's Guide");
                }
                joypad.apply(event);
            }
            for (const auto& [source, sticks] : pads->sticks()) {
                joypad.sticks(source, sticks);
            }
        }
        const bool touchShown = touch && !padConnected && !waiting;
        if (touch) {
            const WindowSurface surface = presenter->surface();
            if (surface.width != laidOut.width || surface.height != laidOut.height ||
                surface.dp != laidOut.dp) {
                touch->layout(surface);
                laidOut = surface;
                joypad.hold("touch", {});
            }
            const bool pausing = touch->pressed(gamepad::Button::Guide);
            bool changed = false;
            if (touchShown) {
                std::vector<Finger> fingers = std::exchange(liftNext, {});
                fingers.insert(fingers.end(), input.fingers.begin(), input.fingers.end());
                std::vector<std::uint64_t> wentDown;
                for (const Finger& finger : fingers) {
                    // A tap shorter than a frame is held for one, so the game sees it.
                    if (finger.phase == Finger::Phase::Up &&
                        std::ranges::find(wentDown, finger.id) != wentDown.end()) {
                        liftNext.push_back(finger);
                        continue;
                    }
                    if (finger.phase == Finger::Phase::Down) {
                        wentDown.push_back(finger.id);
                    }
                    changed = touch->finger(finger) || changed;
                }
            } else {
                liftNext.clear();
                changed = touch->releaseAll();
            }
            if (changed) {
                const std::vector<gamepad::Button> held = touch->held();
                joypad.hold("touch", held);
                lucent::info("retro", "touch holds {}", buttonList(held));
                if (!pausing && touch->pressed(gamepad::Button::Guide)) {
                    openShellMenu("the pause button");
                }
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
        if (waiting) {
            next = Clock::now() + interval;
            std::this_thread::sleep_until(next);
            continue;
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
        std::optional<OverlayImage> overlay;
        if (touchShown) {
            overlay = touch->image();
        }
        try {
            presenter->present(coreImage.value_or(FrameImage{
                                   video.rgba(), video.width(), video.height(), aspect, {}}),
                               coreImageFresh || pixelsFresh, hardware.takeSync(),
                               overlay ? &*overlay : nullptr);
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
