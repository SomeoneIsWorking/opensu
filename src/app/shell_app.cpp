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

#include "lucent/log.h"

namespace iideck::app {
namespace {

/// How long a direction must be held before it repeats, then how fast, which is
/// how a console menu behaves.
constexpr int repeatDelayFrames = 22;
constexpr int repeatIntervalFrames = 7;

/// A short human summary of a title's state, shown in the details toast.
std::string describe(const library::Game& game)
{
    std::ostringstream out;
    out << game.title << " · " << library::label(game.source);
    if (game.playtimeMinutes > 0) {
        out << " · " << (game.playtimeMinutes / 60) << " h played";
    } else {
        out << " · never played";
    }
    return out.str();
}

std::string clockNow()
{
    const std::time_t now = std::time(nullptr);
    std::tm parts{};
    localtime_r(&now, &parts);
    char text[16]{};
    std::strftime(text, sizeof(text), "%H:%M", &parts);
    return text;
}

} // namespace

ShellApp::ShellApp(Settings settings)
    : settings_{std::move(settings)}
    , catalog_{library::makeCatalog(library::readConfig())}
    , shell_{settings_.width, settings_.height}
{
    shell_.setClock(clockNow());
}

void ShellApp::flipVertical(Image& image)
{
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

void ShellApp::reloadCatalog()
{
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
    shell_.setStatus(std::to_string(games_.size()) + " games · "
        + std::to_string(installed) + " installed");
}

void ShellApp::pushCatalogToShell() { shell_.setCatalog(games_); }

void ShellApp::handleEvents(const std::vector<gamepad::Event>& events)
{
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
            if (event.pressed) {
                actOn(event.button);
            }
            break;
        }
    }
}

void ShellApp::actOn(gamepad::Button button)
{
    switch (button) {
    case gamepad::Button::Up:
        if (!shell_.moveFocus(0, -1)) {
            shell_.movePage(-1);
        }
        break;
    case gamepad::Button::Down:
        if (!shell_.moveFocus(0, 1)) {
            shell_.movePage(1);
        }
        break;
    case gamepad::Button::Left:
        if (!shell_.moveFocus(-1, 0)) {
            shell_.movePage(-1);
        }
        break;
    case gamepad::Button::Right:
        if (!shell_.moveFocus(1, 0)) {
            shell_.movePage(1);
        }
        break;
    case gamepad::Button::A:
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

void ShellApp::launchFocused()
{
    const library::Game* game = shell_.focusedGame();
    if (game == nullptr) {
        return;
    }
    if (game->launch.empty()) {
        shell_.setToast("no emulator configured for " + std::string{library::label(game->source)}, true);
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
    shell_.setToast("starting " + copy.title);

    std::thread{[this, copy] {
        std::string failure;
        // Hiding and showing the window across a thread boundary is safe here:
        // raylib's window calls are queued onto the main loop.
        launch::Handoff::start(copy, [] {}, [] {}, failure);
        {
            const std::lock_guard lock{launchMutex_};
            launchRunning_ = false;
        }
        if (!failure.empty()) {
            lucent::error("launch", "{}", failure);
            shell_.setToast(failure, true);
        }
    }}.detach();
}

void ShellApp::showDetails()
{
    if (const library::Game* game = shell_.focusedGame(); game != nullptr) {
        shell_.setToast(describe(*game));
    }
}

void ShellApp::refreshClock() { shell_.setClock(clockNow()); }

int ShellApp::run()
{
    reloadCatalog();

    InitWindow(settings_.width, settings_.height, "iideck");
    // Textures need a GL context, so artwork is loaded only once the window is up.
    shell_.loadArtwork();
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    int clockFrames = 0;
    while (!closeRequested_ && !WindowShouldClose()) {
        std::vector<gamepad::Event> events;
        pad_.poll(events);
        handleEvents(events);

        shell_.draw();

        if (++clockFrames >= 600) {
            clockFrames = 0;
            refreshClock();
        }
        shell_.tickToast();
    }

    CloseWindow();
    return 0;
}

bool ShellApp::renderToFile(const std::string& path)
{
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
    lucent::info("render", "loaded artwork for {} of {} tiles", shell_.loadedArtwork(), shell_.tiles().size());

    RenderTexture target = LoadRenderTexture(settings_.width, settings_.height);
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
    const bool ok = ExportImage(frame, path.c_str());
    UnloadTexture(target.texture);
    CloseWindow();

    if (ok) {
        lucent::info("render", "wrote {}", path);
    } else {
        lucent::error("render", "could not write {}", path);
    }
    return ok;
}

} // namespace iideck::app