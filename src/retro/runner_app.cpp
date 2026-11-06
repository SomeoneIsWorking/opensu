#include "runner_app.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_scancode.h>

#include "arcade_cabinet.hpp"
#include "audio_output.hpp"
#include "audio_sink.hpp"
#include "control_commands.hpp"
#include "control_server.hpp"
#include "core.hpp"
#include "core_environment.hpp"
#include "fileio/atomic_write.hpp"
#include "gamepad/pad_source.hpp"
#include "hardware_render.hpp"
#include "joypad.hpp"
#include "librashader_library.hpp"
#include "lucent/log.h"
#include "presenter.hpp"
#include "resume_state.hpp"
#include "runner_touch.hpp"
#include "save_memory.hpp"
#include "save_paths.hpp"
#include "save_state.hpp"
#include "screen_turn.hpp"
#include "shell_handoff.hpp"
#include "touch/panel_text.hpp"
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

/// Tells the shell the picture's size, or none when the core turns it, whose frames then differ.
void reportPicture(ControlServer& control, const FrameImage& frame, unsigned turns,
                   const settings::VideoSettings& video) {
    const float height = static_cast<float>(frame.height);
    const float own = height > 0.0F ? static_cast<float>(frame.width) / height : 0.0F;
    control.reportPicture(ShownPicture{.width = turns == 0 ? frame.width : 0U,
                                       .height = turns == 0 ? frame.height : 0U,
                                       .aspect = frame.aspect > 0.0F ? frame.aspect : own,
                                       .nearest = video.sampling == settings::Sampling::Nearest});
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

/// The control channel's pad holds `held` alone: what it held before is let go.
/// How long a frame waits for the other machines before the loop serves its window again.
constexpr std::chrono::milliseconds netplayWait{2};

std::string_view endOf(netplay::SessionEnd end) {
    switch (end) {
    case netplay::SessionEnd::None:
        return "left";
    case netplay::SessionEnd::Left:
        return "the other player left";
    case netplay::SessionEnd::Quiet:
        return "the other player went quiet";
    case netplay::SessionEnd::Desync:
        return "the games differ";
    }
    return "ended";
}

void holdControlPad(Joypad& joypad, const ControlInput& held) {
    const std::string source = held.pad.empty() ? "control" : "control:" + held.pad;
    if (held.leave) {
        gamepad::Event gone;
        gone.kind = gamepad::Event::Kind::Disconnected;
        gone.source = source;
        joypad.apply(gone);
        return;
    }
    joypad.hold(source, held.buttons);
    joypad.sticks(source, held.sticks);
    lucent::debug("retro", "control pad holds {} buttons, left stick {},{}, right stick {},{}",
                  held.buttons.size(), held.sticks.leftX, held.sticks.leftY, held.sticks.rightX,
                  held.sticks.rightY);
}

/// The control channel when the shell asked for one and it listens; none otherwise.
std::unique_ptr<ControlServer> openControl(const RunnerOptions& options, const Core& core,
                                           const NetplayLink* netplay) {
    if (options.control.empty()) {
        return nullptr;
    }
    auto control =
        std::make_unique<ControlServer>(options.control, core.name(), options.rom.string());
    if (netplay != nullptr) {
        control->announceNetplayPort(netplay->port());
    }
    if (!control->start()) {
        return nullptr;
    }
    return control;
}

/// Rotate screen on a keyboard.
constexpr int rotateKey = SDL_SCANCODE_F9;

/// Asks for the shell's menu, which comes over the game.
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
    Joypad joypad{options_.netplay ? 1 : options_.players};
    std::unique_ptr<NetplayLink> netplay;
    if (options_.netplay) {
        try {
            netplay = std::make_unique<NetplayLink>(*options_.netplay, joypad);
        } catch (const std::system_error& failure) {
            lucent::error("retro", "netplay cannot start: {}", failure.what());
            if (!options_.control.empty()) {
                ControlServer::reportLoadFailure(options_.control, failure.what());
            }
            return RunnerExit::LoadFailed;
        }
        environment.joinNetplay(netplay->slot(), options_.netplay->clock);
        lucent::info("retro", "netplay: player {} of {} on UDP port {}", netplay->slot() + 1,
                     options_.netplay->session.players, netplay->port());
    }
    std::unique_ptr<Core> core;
    try {
        const PortInput& input = netplay ? static_cast<const PortInput&>(*netplay) : joypad;
        core = std::make_unique<Core>(options_.core, CoreHost{environment, video, sink, input});
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
    bool nativeWindow = false;
    unsigned fittedWidth = 0;
    unsigned fittedHeight = 0;
    Clock::duration interval = frameInterval(core->avInfo().timing.fps);
    sink.setCoreRate(core->avInfo().timing.sample_rate);

    SaveMemory saves{saveMemoryFile(options_.saves, options_.rom)};
    try {
        saves.load(core->saveRam());
    } catch (const SaveError& failure) {
        lucent::error("retro", "{}", failure.what());
        if (!options_.control.empty()) {
            ControlServer::reportLoadFailure(options_.control, failure.what());
        }
        return RunnerExit::LoadFailed;
    }
    SaveState state{saveStateFile(options_.saves, options_.rom)};
    ResumeState resume{resumeStateFile(options_.saves, options_.rom), coreName(options_.core)};

    const bool handoff = options_.embedded || (shell_handoff::active() && !options_.hidden);
    if (handoff) {
        // Back asks for the shell's menu instead of closing the Activity, and the loop goes on
        // under the shell, serving its save and load requests.
        SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
        SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
    }
    std::unique_ptr<Presenter> presenter;
    try {
        presenter = std::make_unique<Presenter>(PresenterOptions{
            .hidden = options_.hidden,
            .fullscreen = options_.fullscreen,
            .hosted = options_.embedded && !options_.hidden,
            .width = options_.width,
            .height = options_.height,
            .hiddenDp = options_.touchDp.value_or(1.0F),
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
    // After a GPU core's context is up, which its state may need.
    const ResumeReport resumed = resume.resume(*core);
    if (netplay) {
        netplay->attach(*core, environment);
    }
    const std::unique_ptr<AudioOutput> audio = openAudio(sink, options_.hidden);
    const std::unique_ptr<gamepad::PadSource> pads =
        options_.hidden || options_.embedded ? nullptr : openPads();
    const std::unique_ptr<ControlServer> control = openControl(options_, *core, netplay.get());
    if (control) {
        control->reportResume(resumed);
    }
    ScreenTurn screen{*presenter, control.get(), options_.video.turned};
    ControlCommands commands{{.core = *core,
                              .state = state,
                              .saves = saves,
                              .resume = resume,
                              .presenter = *presenter,
                              .options = environment.options(),
                              .screen = screen,
                              .netplay = netplay != nullptr}};

    std::unique_ptr<RunnerTouch> touch =
        handoff || options_.touchDp ? std::make_unique<RunnerTouch>(options_.touchArt, joypad)
                                    : nullptr;
    if (touch && options_.arcade) {
        touch->cabinet(*options_.arcade);
    }
    if (control && options_.arcade) {
        control->reportPanel(touch::panelText(panelOf({}, *options_.arcade)));
    }
    bool padConnected = pads && !pads->connected().empty();
    /// Another Activity has the screen's input, usually the shell over the game, or the shell
    /// says a menu of its own is up.
    // A hosted game is covered until the shell has told it what it needs to show its panel.
    bool covered = options_.embedded;
    bool announcedShown = false;
    /// Out of sight: the game waits.
    bool waiting = false;
    /// Under the Guide: the core and its sound stop, the picture stays.
    bool holding = false;

    RunnerExit exit = RunnerExit::Quit;
    std::uint64_t frame = 0;
    /// Whether the last frame came as the core's Vulkan image rather than pixels.
    bool showingCoreImage = false;
    Clock::time_point next = Clock::now();
    Clock::time_point nextSave = next + saveInterval;
    FrameStages stages;
    const auto heardKey = [&joypad, &screen](int scancode, bool down) {
        if (scancode != rotateKey) {
            joypad.key(scancode, down);
        } else if (down) {
            screen.toggle();
        }
    };
    while (!options_.frames || frame < *options_.frames) {
        const WindowInput input = presenter->poll();
        if (input.quit) {
            break;
        }
        if (handoff && input.focused) {
            covered = !*input.focused;
            lucent::info("retro", "the game is {}", covered ? "covered" : "in front");
        }
        if (control) {
            for (const HostInput& host : control->takeHost()) {
                if (host.covered) {
                    covered = *host.covered;
                    lucent::info("retro", "the game is {}", covered ? "covered" : "in front");
                }
                if (host.pads) {
                    padConnected = *host.pads > 0;
                }
                if (host.screen) {
                    presenter->place(*host.screen);
                }
                if (host.key) {
                    heardKey(host.key->scancode, host.key->down);
                }
            }
        }
        // A shell in the background cannot say so, and a hosted surface going says it for it.
        if (const bool outOfSight = (covered || options_.embedded) && !presenter->onScreen();
            outOfSight != waiting) {
            waiting = outOfSight;
            lucent::info("retro", "the game {}", waiting ? "waits out of sight" : "goes on");
        }
        if (!options_.hidden) {
            for (const WindowInput::Key& key : input.keys) {
                if (handoff && key.scancode == SDL_SCANCODE_AC_BACK) {
                    if (key.down) {
                        openShellMenu("Back");
                    }
                    continue;
                }
                heardKey(key.scancode, key.down);
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
            for (const auto& [source, axes] : pads->axes()) {
                joypad.sticks(source, axes.sticks);
            }
        }
        const bool touchShown = touch && !padConnected && !covered && !waiting;
        if (touch) {
            touch->sized(presenter->video().touchSize);
        }
        if (touch && touch->follow(presenter->surface(), touchShown, input.fingers)) {
            openShellMenu("the pause button");
        }
        if (touch && touch->takeRotate()) {
            screen.toggle();
        }
        presenter->reserve(touchShown ? static_cast<std::uint32_t>(touch->band()) : 0U);

        if (control) {
            for (const ControlInput& held : control->takeInput()) {
                holdControlPad(joypad, held);
            }
            for (const touch::Finger& finger : control->takeFingers()) {
                presenter->inject(finger);
            }
            control->serve([&commands](const ControlRequest& request) {
                return commands.perform(request);
            });
        }
        if (waiting || commands.suspended()) {
            next = Clock::now() + interval;
            std::this_thread::sleep_until(next);
            continue;
        }
        // In front again without the Guide closing (Home and back), the game must not stay held.
        if (const bool held = commands.held() && (!handoff || covered); held != holding) {
            holding = held;
            audio->hold(holding);
            lucent::info("retro", "the game {}", holding ? "holds under the Guide" : "goes on");
        }
        if (netplay) {
            const bool ready = netplay->ready(*core, netplayWait);
            if (netplay->ended() != netplay::SessionEnd::None) {
                break;
            }
            if (!ready) {
                continue;
            }
        }
        const Clock::time_point started = Clock::now();
        if (!holding) {
            core->run();
            stages.add(FrameStages::Stage::Core, Clock::now() - started);
            ++frame;
            if (touch) {
                touch->ran();
            }
            if (netplay) {
                netplay->ran(*core);
            }
        }
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
        if (!holding) {
            audio->service(now);
        }
        stages.add(FrameStages::Stage::Audio, Clock::now() - now);
        const bool coreImageFresh = hardware.takeFresh();
        const bool pixelsFresh = video.takeFresh();
        if (coreImageFresh || pixelsFresh) {
            showingCoreImage = coreImageFresh;
        }
        const float aspect = aspectOf(geometry);
        const std::optional<FrameImage> coreImage =
            showingCoreImage ? hardware.frame(aspect) : std::nullopt;
        FrameImage shown =
            coreImage.value_or(FrameImage{video.rgba(), video.width(), video.height(), aspect, {}});
        shown.interval = std::chrono::duration_cast<std::chrono::nanoseconds>(interval);
        if (control) {
            reportPicture(*control, shown, environment.rotation(), presenter->video());
            nativeWindow = control->takeNative() || nativeWindow;
            if (nativeWindow && (shown.width != fittedWidth || shown.height != fittedHeight)) {
                fittedWidth = shown.width;
                fittedHeight = shown.height;
                presenter->fitWindow(shown.width, shown.height);
            }
        }
        std::optional<touch::OverlayImage> overlay;
        if (const auto named = environment.takeInputDescriptors()) {
            if (touch) {
                touch->named(*named);
            }
            if (control && options_.arcade) {
                control->reportPanel(touch::panelText(panelOf(*named, *options_.arcade)));
            }
        }
        if (touch) {
            if (shown.width != 0) {
                touch->showing(presenter->picture(shown));
            }
            if (control && touch->takeMoved()) {
                control->publishTouch(touch->places());
            }
        }
        if (touchShown) {
            overlay = touch->image();
        }
        try {
            presenter->present(shown, coreImageFresh || pixelsFresh, hardware.takeSync(),
                               overlay ? &*overlay : nullptr, stages);
        } catch (const VulkanError& failure) {
            lucent::error("retro", "{}", failure.what());
            exit = RunnerExit::PresenterFailed;
            break;
        } catch (const ShaderError& failure) {
            lucent::error("retro", "{}", failure.what());
            exit = RunnerExit::PresenterFailed;
            break;
        }
        if (control && !announcedShown && presenter->shown()) {
            announcedShown = true;
            control->reportShown();
            lucent::info("retro", "the first frame is on screen");
        }
        if (now >= nextSave) {
            saves.flush(core->saveRam());
            nextSave = now + saveInterval;
        }

        next += interval;
        const Clock::time_point slept = Clock::now();
        if (slept - next > interval * maxLateFrames) {
            next = slept;
        } else {
            std::this_thread::sleep_until(next);
        }
        stages.add(FrameStages::Stage::Sleep, Clock::now() - slept);
        stages.endFrame(started, static_cast<double>(sink.fill()) * 1000.0 / sink.outputRate());
    }
    const auto millisecondsOf = [&sink](std::size_t frames) {
        return frames * 1000 / static_cast<std::size_t>(sink.outputRate());
    };
    if (netplay) {
        netplay->leave();
        lucent::info("retro",
                     "netplay: {} after {} frames, {} stalls, {} frames run again, {} state "
                     "checks matched",
                     endOf(netplay->ended()), frame, netplay->stalls(), netplay->replayed(),
                     netplay->matched());
    }
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
