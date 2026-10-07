#include "shell_app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

#include "config/config.hpp"
#include "lucent/log.h"

namespace iideck::app {
namespace {

/// A short human summary of a title's state, shown in the details toast.
std::string describe(const library::Game& game) {
    std::ostringstream out;
    out << game.title << " · " << library::label(game.source);
    if (game.playtimeMinutes > 0) {
        out << " · " << (game.playtimeMinutes / 60) << " h played";
    } else {
        out << " · never played";
    }
    return out.str();
}

ui::ServiceState serviceState(launch::SteamState state) {
    switch (state) {
    case launch::SteamState::Initializing:
        return ui::ServiceState::Starting;
    case launch::SteamState::Ready:
        return ui::ServiceState::Ready;
    case launch::SteamState::Failed:
        return ui::ServiceState::Failed;
    case launch::SteamState::Blocked:
        return ui::ServiceState::Blocked;
    case launch::SteamState::Stopped:
        break;
    }
    return ui::ServiceState::Hidden;
}

std::string clockNow() {
    const std::time_t now = std::time(nullptr);
    std::tm parts{};
    localtime_r(&now, &parts);
    char text[16]{};
    std::strftime(text, sizeof(text), "%H:%M", &parts);
    return text;
}

} // namespace

ShellApp::ShellApp(Settings settings)
    : settings_{std::move(settings)}, catalog_{library::makeCatalog(config::read())},
      shell_{settings_.width, settings_.height, settings_.homeMode},
      steam_{steam::Client::Options{config::read().home, config::read().executablePath,
                                    config::read().session}},
      handoff_{config::read().executablePath, config::read().session, steam_} {
    shell_.setClock(clockNow());
    // The platform table frames every tile. Loaded before the catalog, because the
    // frames are resolved as the grid is laid out.
    shell_.setPlatforms(ui::Platforms::load(ui::defaultPlatformRoot()));

    // SDL reports any device with buttons as a gamepad, which on a desktop
    // includes a multimedia keyboard. raylib cannot tell the two apart without
    // input, so a name filter lets the player name their controller.
    if (!config::read().gamepadNameFilter.empty()) {
        pad_.setNameFilter({config::read().gamepadNameFilter});
        lucent::info("gamepad", "only controllers matching \"{}\" are accepted",
                     config::read().gamepadNameFilter);
    }
}

void ShellApp::flipVertical(Image& image) {
    const int stride = image.width * 4;
    std::vector<unsigned char> row(static_cast<std::size_t>(stride));
    auto* pixels = static_cast<unsigned char*>(image.data);
    for (int y = 0; y < image.height / 2; ++y) {
        unsigned char* top = pixels + static_cast<std::size_t>(y) * stride;
        unsigned char* bottom = pixels + static_cast<std::size_t>(image.height - 1 - y) * stride;
        std::memcpy(row.data(), top, static_cast<std::size_t>(stride));
        std::memcpy(top, bottom, static_cast<std::size_t>(stride));
        std::memcpy(bottom, row.data(), static_cast<std::size_t>(stride));
    }
}

void ShellApp::reloadCatalog() {
    std::vector<std::string> problems;
    games_ = catalog_.refresh(problems);
    for (const std::string& problem : problems) {
        lucent::warn("catalog", "{}", problem);
    }
    lucent::info("catalog", "{} games loaded", games_.size());

    shell_.setCatalog(games_);
    std::size_t installed = 0;
    for (const library::Game& game : games_) {
        if (game.installed) {
            ++installed;
        }
    }
    shell_.setStatus(std::to_string(games_.size()) + " games · " + std::to_string(installed) +
                     " installed");
}

void ShellApp::pushCatalogToShell() {
    shell_.setCatalog(games_);
}

void ShellApp::handleEvents(const std::vector<gamepad::Event>& events) {
    for (const gamepad::Event& event : events) {
        switch (event.kind) {
        case gamepad::Event::Kind::Connected:
            lucent::info("gamepad", "controller ready: {}", event.device);
            break;
        case gamepad::Event::Kind::Disconnected:
            shell_.setToast("controller disconnected", true);
            break;
        case gamepad::Event::Kind::Axis:
            break;
        case gamepad::Event::Kind::Button:
            if (event.button == gamepad::Button::Guide) {
                if (event.pressed) {
                    guideHeldSince_ = std::chrono::steady_clock::now();
                } else {
                    guideHeldSince_.reset();
                }
            }
            if (event.pressed) {
                actOn(event.button);
            }
            break;
        }
    }
}

/// Keyboard input, so the shell is usable without a controller. Every key maps
/// to the button it stands in for rather than to a shell command of its own, so
/// there is one set of actions and the keyboard is a second way to reach it.
void ShellApp::handleKeyboard() {
    struct Binding {
        int key;
        gamepad::Button button;
        bool edge;
    };
    static constexpr Binding kBindings[]{
        {KEY_UP, gamepad::Button::Up, false},       {KEY_W, gamepad::Button::Up, false},
        {KEY_DOWN, gamepad::Button::Down, false},   {KEY_S, gamepad::Button::Down, false},
        {KEY_LEFT, gamepad::Button::Left, false},   {KEY_A, gamepad::Button::Left, false},
        {KEY_RIGHT, gamepad::Button::Right, false}, {KEY_D, gamepad::Button::Right, false},
        {KEY_ENTER, gamepad::Button::A, true},      {KEY_SPACE, gamepad::Button::A, true},
        {KEY_Y, gamepad::Button::Y, true},          {KEY_R, gamepad::Button::R1, true},
        {KEY_F, gamepad::Button::X, true},          {KEY_LEFT_BRACKET, gamepad::Button::L1, true},
        {KEY_E, gamepad::Button::Start, true},      {KEY_ESCAPE, gamepad::Button::B, true},
    };

    std::array<bool, std::size(kBindings)> held{};
    for (std::size_t i = 0; i < std::size(kBindings); ++i) {
        const bool down = IsKeyDown(kBindings[i].key);
        // A directional key repeats while held; an action key fires once.
        if (down && (!kBindings[i].edge || !held[i])) {
            actOn(kBindings[i].button);
        }
        held[i] = down;
    }

    if (IsKeyPressed(KEY_Q)) {
        requestClose();
    }
}

void ShellApp::actOn(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
        shell_.moveFocus(ui::Direction::Up);
        break;
    case gamepad::Button::Down:
        shell_.moveFocus(ui::Direction::Down);
        break;
    case gamepad::Button::Left:
        shell_.moveFocus(ui::Direction::Left);
        break;
    case gamepad::Button::Right:
        shell_.moveFocus(ui::Direction::Right);
        break;
    case gamepad::Button::A:
        shell_.pressFocused();
        launchFocused();
        break;
    case gamepad::Button::Y:
    case gamepad::Button::Select:
        showDetails();
        break;
    case gamepad::Button::X:
        reloadCatalog();
        shell_.setToast("library refreshed");
        break;
    case gamepad::Button::L1:
        // iiSU's L1/R1 cycle sections; iideck has none, so they turn WiiSu pages.
        shell_.movePage(-1);
        break;
    case gamepad::Button::R1:
        shell_.movePage(1);
        break;
    case gamepad::Button::Start:
        shell_.resetFocus();
        break;
    case gamepad::Button::B:
        // Back out of a launch, or return from a game.
        break;
    default:
        break;
    }
}

void ShellApp::launchFocused() {
    const library::Game* game = shell_.focusedGame();
    if (game == nullptr) {
        return;
    }
    if (game->launch.empty()) {
        shell_.setToast("no emulator configured for " + std::string{library::label(game->source)},
                        true);
        return;
    }
    {
        const std::lock_guard lock{launchMutex_};
        if (launchRunning_) {
            shell_.setToast("a game is already running", true);
            return;
        }
        launchRunning_ = true;
    }

    // The copy outlives this call because the handoff thread reads it.
    const library::Game copy = *game;
    const bool waitingForSteam =
        copy.source == library::Source::Steam && steam_.state() == launch::SteamState::Initializing;
    shell_.setToast(waitingForSteam ? "waiting for Steam" : "starting " + copy.title);

    std::thread{[this, copy] {
        std::string failure;
        handoff_.start(
            copy,
            [this] {
                // raylib's window calls belong to the thread that owns the GL context, so
                // the handoff thread only raises a flag and the loop does the work. Hiding
                // a window off-thread is not something raylib supports.
                requestWindowVisible(false);
            },
            [this] {
                requestWindowVisible(true);
            },
            failure);
        {
            const std::lock_guard lock{launchMutex_};
            launchRunning_ = false;
        }
        if (!failure.empty()) {
            lucent::error("launch", "{}", failure);
            requestToast(failure, true);
        } else {
            requestToast(copy.title + " closed", false);
        }
    }}.detach();
}

void ShellApp::serviceForceClose() {
    if (!guideHeldSince_ || std::chrono::steady_clock::now() - *guideHeldSince_ < forceCloseHold) {
        return;
    }
    guideHeldSince_.reset();
    {
        const std::lock_guard lock{launchMutex_};
        if (!launchRunning_) {
            return;
        }
    }
    lucent::warn("launch", "Guide held; force-closing the running launch");
    handoff_.forceClose();
}

void ShellApp::showDetails() {
    if (const library::Game* game = shell_.focusedGame(); game != nullptr) {
        shell_.setToast(describe(*game));
    }
}

void ShellApp::refreshClock() {
    shell_.setClock(clockNow());
}

bool ShellApp::renderFrameToPng(std::string& png) {
    const RenderTexture target = LoadRenderTexture(settings_.width, settings_.height);
    if (target.id == 0) {
        lucent::error("render", "could not create an offscreen target");
        return false;
    }

    BeginTextureMode(target);
    shell_.draw();
    EndTextureMode();

    // A render texture has OpenGL's bottom-up origin, so the exported image is
    // the frame upside down. Flipping the rows here keeps draw() identical
    // between the window and this path.
    Image frame = LoadImageFromTexture(target.texture);
    flipVertical(frame);

    // raylib can only encode to a file, so the bytes are written there and read
    // back. The name carries the process id, so two shells on one machine do
    // not fight over it.
    const std::string path = (std::filesystem::temp_directory_path() /
                              ("iideck-frame-" + std::to_string(::getpid()) + ".png"))
                                 .string();
    const bool written = ExportImage(frame, path.c_str());
    UnloadImage(frame);
    UnloadTexture(target.texture);
    if (!written) {
        lucent::error("render", "could not encode the frame");
        return false;
    }

    std::ifstream in{path, std::ios::binary};
    png.assign(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});
    in.close();
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return !png.empty();
}

ShellSnapshot ShellApp::snapshot() const {
    const std::lock_guard lock{stateMutex_};
    return published_;
}

void ShellApp::inject(gamepad::Button button) {
    const std::lock_guard lock{injectedMutex_};
    injected_.push_back(button);
}

void ShellApp::requestClose() {
    closeRequested_.store(true);
}

void ShellApp::requestWindowVisible(bool visible) {
    windowVisible_.store(visible);
}

void ShellApp::requestToast(std::string text, bool isError) {
    {
        const std::lock_guard lock{toastMutex_};
        pendingToast_ = std::move(text);
        pendingToastError_ = isError;
        hasPendingToast_ = true;
    }
}

bool ShellApp::captureFrame(std::string& png) {
    std::unique_lock lock{stateMutex_};
    if (capturePending_) {
        // One frame request at a time: two callers racing for the context would
        // interleave, and the second would get the first's answer.
        return false;
    }
    capturePending_ = true;
    captureResult_.clear();
    // The loop answers within a frame or two. The bound stops a caller hanging
    // forever if the loop has already exited.
    captureAnswered_.wait_for(lock, std::chrono::seconds{5}, [this] {
        return !capturePending_;
    });
    if (capturePending_) {
        // Timed out, so the request is abandoned rather than left set.
        capturePending_ = false;
        return false;
    }
    png = captureResult_;
    captureResult_.clear();
    return !png.empty();
}

void ShellApp::publishSnapshot() {
    ShellSnapshot next;
    next.games = games_.size();
    for (const library::Game& game : games_) {
        if (game.installed) {
            ++next.installed;
        }
    }
    if (const library::Game* focused = shell_.focusedGame(); focused != nullptr) {
        next.focusedId = focused->id;
        next.focusedTitle = focused->title;
    }
    next.focusIndex = shell_.focusIndex();
    next.page = static_cast<std::size_t>(std::max(shell_.page(), 0));
    next.pageCount = static_cast<std::size_t>(std::max(shell_.pageCount(), 1));
    next.status = shell_.status();
    next.toast = shell_.toast();
    next.toastIsError = shell_.toastIsError();
    {
        const std::lock_guard lock{launchMutex_};
        next.launching = launchRunning_;
    }
    // What the loop has actually done to the window, not what was asked for, so a
    // request that never reached the loop cannot read as hidden.
    next.windowVisible = windowShown_;
    next.steam = std::string{launch::name(steam_.state())};

    const std::lock_guard lock{stateMutex_};
    published_ = std::move(next);
}

void ShellApp::serviceControlRequests() {
    // Buttons injected over the channel take the same path as a real press, so
    // what the channel exercises is the shell's own handling.
    std::vector<gamepad::Button> queued;
    {
        const std::lock_guard lock{injectedMutex_};
        queued.swap(injected_);
    }
    for (const gamepad::Button button : queued) {
        std::vector<gamepad::Event> press{gamepad::Event{
            .kind = gamepad::Event::Kind::Button, .button = button, .pressed = true}};
        handleEvents(press);
    }

    bool wanted = false;
    {
        const std::lock_guard lock{stateMutex_};
        wanted = capturePending_;
    }
    if (!wanted) {
        return;
    }

    std::string png;
    const bool ok = renderFrameToPng(png);
    const std::lock_guard lock{stateMutex_};
    captureResult_ = ok ? std::move(png) : std::string{};
    capturePending_ = false;
    captureAnswered_.notify_all();
}

/// Applies what the launch thread and the control channel asked for. Everything
/// that touches the window or the shell happens here, on the loop's thread.
void ShellApp::serviceRequests() {
    const bool visible = windowVisible_.load();
    if (visible != windowShown_) {
        windowShown_ = visible;
        // raylib has no ShowWindow or HideWindow: hiding is a window state flag,
        // and showing is clearing it.
        if (visible) {
            ClearWindowState(FLAG_WINDOW_HIDDEN);
        } else {
            SetWindowState(FLAG_WINDOW_HIDDEN);
        }
    }

    if (hasPendingToast_) {
        std::string text;
        bool isError = false;
        {
            const std::lock_guard lock{toastMutex_};
            text = std::move(pendingToast_);
            isError = pendingToastError_;
            pendingToast_.clear();
            hasPendingToast_ = false;
        }
        shell_.setToast(std::move(text), isError);
    }
}

int ShellApp::run() {
    reloadCatalog();

    // Resizable, because the layout is computed from the window size rather than
    // baked at 1280x800, and because a fixed window on a scaled desktop is
    // unusable. The scale factor is applied by the window manager on the way in,
    // so what the shell draws in is already in its own pixels.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(settings_.width, settings_.height, "iideck");
    SetWindowMinSize(960, 600);
    // Textures need a GL context, so artwork is loaded only once the window is up.
    shell_.loadArtwork();
    lucent::info("ui", "artwork loaded for {} of {} tiles", shell_.loadedArtwork(),
                 shell_.tiles().size());
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    // The control channel is part of the product, not a debug flag: it is how an
    // automated run drives the shell without a controller.
    if (settings_.controlChannel) {
        control_ = std::make_unique<ControlChannel>(*this, settings_.controlPort);
        control_->start();
    }

    // Steam comes up in the background while the shell is already usable. A machine
    // without a Steam install has nothing to start, and shows no Steam icon.
    const config::Config& config = config::read();
    if (!library::steam::Library::discover(config.home, config.steamRoots).roots().empty()) {
        steam_.start();
    }

    int clockFrames = 0;
    while (!closeRequested_.load() && !WindowShouldClose()) {
        // A resize changes the framebuffer, so the layout has to be recomputed
        // before anything is drawn into it. Checked every frame because there is
        // no resize callback worth relying on across platforms.
        if (IsWindowResized()) {
            shell_.setSize(GetScreenWidth(), GetScreenHeight());
        }

        std::vector<gamepad::Event> events;
        pad_.poll(events);
        handleEvents(events);
        handleKeyboard();
        serviceForceClose();

        serviceControlRequests();
        serviceRequests();
        shell_.setSteamState(serviceState(steam_.state()));
        shell_.tick(std::chrono::steady_clock::now());
        publishSnapshot();
        if (windowVisible_.load()) {
            shell_.draw();
        }

        if (++clockFrames >= 600) {
            clockFrames = 0;
            refreshClock();
        }
    }

    // Anything waiting on a frame will never get one now.
    {
        const std::lock_guard lock{stateMutex_};
        capturePending_ = false;
        captureAnswered_.notify_all();
    }
    if (control_) {
        control_->stop();
    }
    CloseWindow();
    return 0;
}

bool ShellApp::renderToFile(const std::string& path) {
    if (games_.empty()) {
        reloadCatalog();
    } else {
        pushCatalogToShell();
    }

    // raylib needs a GL context before any texture work, and a context needs a
    // window. The window is never shown and nothing is presented, so a render
    // still lands on no screen.
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(settings_.width, settings_.height, "iideck render");
    shell_.loadArtwork();
    lucent::info("render", "loaded artwork for {} of {} tiles", shell_.loadedArtwork(),
                 shell_.tiles().size());
    // A still frame shows the grid at rest, after its entrance.
    shell_.tick(std::chrono::steady_clock::now());
    shell_.settle();

    std::string png;
    const bool ok = renderFrameToPng(png);
    CloseWindow();
    if (!ok) {
        lucent::error("render", "could not render a frame");
        return false;
    }

    std::ofstream out{path, std::ios::binary};
    out.write(png.data(), static_cast<std::streamsize>(png.size()));
    const bool written = out.good();
    if (written) {
        lucent::info("render", "wrote {}", path);
    } else {
        lucent::error("render", "could not write {}", path);
    }
    return written;
}

} // namespace iideck::app