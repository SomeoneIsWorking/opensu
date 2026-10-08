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
#include <utility>

#include "config/config.hpp"
#include "launcher_status.hpp"
#include "library/titles.hpp"
#include "lucent/log.h"
#include "ui/clock_text.hpp"

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

} // namespace

ShellApp::ShellApp(Settings settings)
    : settings_{std::move(settings)}, catalog_{library::makeCatalog(config::read())},
      shell_{settings_.width, settings_.height, settings_.homeMode},
      steam_{steam::Client::Options{config::read().home, config::read().executablePath,
                                    config::read().session, config::read().steamRoots}},
      gameWindows_{config::read().insideGamescope ? std::make_unique<session::GamescopeWindows>()
                                                  : nullptr},
      handoff_{config::read().executablePath, config::read().session, steam_, gameWindows_.get()},
      signIn_{StoreSignIn::Options{.dataDir = config::read().dataDir}} {
    refreshClock();
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
    library::CatalogSnapshot snapshot = catalog_.refresh();
    games_ = std::move(snapshot.games);
    sources_ = std::move(snapshot.sources);
    artworkStore_.apply(games_);
    const std::vector<library::Console> consoles = library::consoles(games_);
    artworkFetcher_.request(games_, consoles);
    for (const library::Console& console : consoles) {
        const std::filesystem::path glyph = artworkStore_.storedGlyph(console.system);
        if (!glyph.empty()) {
            shell_.setGlyph(console.system, glyph);
        }
    }
    for (const library::SourceStatus& source : sources_) {
        if (source.availability != library::Availability::Ready) {
            lucent::warn("catalog", "{}: {}", library::label(source.source), source.detail);
        }
    }
    lucent::info("catalog", "{} games loaded", games_.size());

    showShelf(shell_.focusIndex());
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
    showShelf(shell_.focusIndex());
}

void ShellApp::showShelf(std::size_t focus) {
    std::vector<library::ShelfItem> shelf = browser_.shelf(games_, sources_);
    artworkStore_.apply(shelf);
    shell_.setShelf(std::move(shelf), focus);
}

void ShellApp::serviceArtwork() {
    for (const artwork::Fetched& fetched : artworkFetcher_.take()) {
        if (fetched.kind == artwork::Fetched::Kind::Glyph) {
            shell_.setGlyph(fetched.id, fetched.artwork);
            continue;
        }
        if (fetched.kind == artwork::Fetched::Kind::Console) {
            shell_.setConsoleArtwork(fetched.id, fetched.artwork);
            continue;
        }
        for (library::Game& game : games_) {
            if (game.id == fetched.id) {
                game.artwork = fetched.artwork;
            }
        }
        shell_.setArtwork(fetched.id, fetched.artwork);
    }
}

void ShellApp::openFolder(const library::Folder& folder) {
    if (const auto* launcher = std::get_if<library::Launcher>(&folder);
        launcher != nullptr && launcher->games == 0) {
        const std::string store{library::label(launcher->source)};
        shell_.setToast(launcher->ready ? "no games in " + store : "sign in to " + store, true);
        return;
    }
    browser_.open(folder, shell_.focusIndex());
    showShelf(0);
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
        case gamepad::Event::Kind::Button:
            if (gamepad::DirectionRepeat::repeats(event.button)) {
                if (event.pressed) {
                    repeat_.press(event.button, std::chrono::steady_clock::now());
                } else {
                    repeat_.release(event.button);
                }
            } else if (event.pressed) {
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
    };
    static constexpr Binding kBindings[]{
        {KEY_UP, gamepad::Button::Up},       {KEY_W, gamepad::Button::Up},
        {KEY_DOWN, gamepad::Button::Down},   {KEY_S, gamepad::Button::Down},
        {KEY_LEFT, gamepad::Button::Left},   {KEY_A, gamepad::Button::Left},
        {KEY_RIGHT, gamepad::Button::Right}, {KEY_D, gamepad::Button::Right},
        {KEY_ENTER, gamepad::Button::A},     {KEY_SPACE, gamepad::Button::A},
        {KEY_Y, gamepad::Button::Y},         {KEY_R, gamepad::Button::R1},
        {KEY_F, gamepad::Button::X},         {KEY_LEFT_BRACKET, gamepad::Button::L1},
        {KEY_E, gamepad::Button::Start},     {KEY_ESCAPE, gamepad::Button::B},
    };

    // Keys report edges like a pad's buttons, so held arrows repeat on the pad's schedule rather
    // than the keyboard's.
    std::vector<gamepad::Event> events;
    for (const Binding& binding : kBindings) {
        if (IsKeyPressed(binding.key)) {
            events.push_back(gamepad::Event{.button = binding.button, .pressed = true});
        } else if (IsKeyReleased(binding.key)) {
            events.push_back(gamepad::Event{.button = binding.button, .pressed = false});
        }
    }
    handleEvents(events);

    if (IsKeyPressed(KEY_Q)) {
        requestClose();
    }
}

void ShellApp::handleGameKeys() {
    if (!gameKeys_) {
        return;
    }
    for (const session::GameShortcut shortcut : gameKeys_->poll()) {
        switch (shortcut) {
        case session::GameShortcut::Guide:
            handleEvents({gamepad::Event{.button = gamepad::Button::Guide, .pressed = true},
                          gamepad::Event{.button = gamepad::Button::Guide, .pressed = false}});
            break;
        }
    }
}

void ShellApp::actOn(gamepad::Button button) {
    if (shell_.inGame()) {
        actInGame(button);
        return;
    }
    if (panelUse_ != PanelUse::None) {
        actOnPanel(button);
        return;
    }
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
        if (const std::optional<library::Folder> folder = shell_.focusedFolder()) {
            openFolder(*folder);
        } else {
            launchFocused();
        }
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
        if (const std::optional<std::size_t> focus = browser_.back()) {
            showShelf(*focus);
        }
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
    if (!game->installed) {
        offerInstall(*game);
        return;
    }
    if (game->launch.empty()) {
        shell_.setToast(
            game->unavailable.empty() ? "no way to start " + game->title : game->unavailable, true);
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
    runningTitle_ = copy.title;
    shell_.launchPanel().open(copy.title);
    panelUse_ = PanelUse::Launch;

    std::vector<std::string> environment = pads_.hold();
    padsHeld_ = true;

    std::thread{[this, copy, environment = std::move(environment)] {
        std::string failure;
        // raylib's window calls belong to the thread that owns the GL context, so the handoff
        // thread only raises flags and the loop does the work.
        const launch::Handoff::Hooks hooks{.hide =
                                               [this] {
                                                   requestGameRunning(true);
                                               },
                                           .show =
                                               [this] {
                                                   requestGameRunning(false);
                                               },
                                           .progress =
                                               [this](const launch::LaunchProgress& progress) {
                                                   requestLaunchProgress(progress);
                                               }};
        handoff_.start(copy, hooks, environment, failure);
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

void ShellApp::actOnPanel(gamepad::Button button) {
    switch (panelUse_) {
    case PanelUse::Launch:
        if (button == gamepad::Button::B) {
            cancelLaunch();
        }
        break;
    case PanelUse::OfferInstall:
        if (button == gamepad::Button::A || button == gamepad::Button::X) {
            const std::size_t choice = button == gamepad::Button::A ? 0 : 1;
            if (choice < offered_.size()) {
                startInstall(offered_[choice]);
            }
        } else if (button == gamepad::Button::B) {
            shell_.launchPanel().close();
            panelUse_ = PanelUse::None;
            offered_.clear();
        }
        break;
    case PanelUse::Install:
        // The download goes on without the panel.
        if (button == gamepad::Button::B) {
            shell_.launchPanel().close();
            panelUse_ = PanelUse::None;
        }
        break;
    case PanelUse::Eula:
        if (button == gamepad::Button::A || button == gamepad::Button::B) {
            const bool accepted = button == gamepad::Button::A;
            install_.decide(accepted);
            if (accepted) {
                panelUse_ = PanelUse::Install;
                shell_.launchPanel().update("Starting the download", std::nullopt);
                shell_.launchPanel().setHints({{"B", "Hide"}});
            } else {
                shell_.launchPanel().close();
                panelUse_ = PanelUse::None;
            }
        }
        break;
    case PanelUse::None:
        break;
    }
}

void ShellApp::startInstall(const library::Game& game) {
    if (install_.start(game)) {
        panelUse_ = PanelUse::Install;
        shell_.launchPanel().update("Starting", std::nullopt);
        shell_.launchPanel().setHints({{"B", "Hide"}});
    } else {
        shell_.launchPanel().close();
        panelUse_ = PanelUse::None;
        shell_.setToast(install_.title() + " is still installing", true);
    }
    offered_.clear();
}

void ShellApp::offerInstall(const library::Game& game) {
    // A store's own page installs that store's copy; elsewhere any store that owns the title will do.
    std::vector<library::Game> copies =
        browser_.inLauncher() ? std::vector<library::Game>{game} : library::copiesOf(games_, game);
    const std::string unsupported = std::string{library::label(copies.front().source)} +
                                    " installs are not supported yet";
    std::erase_if(copies, [](const library::Game& copy) {
        return copy.installed || !Installs::supports(copy.source);
    });
    if (copies.empty()) {
        shell_.setToast(unsupported, true);
        return;
    }
    if (install_.running()) {
        shell_.setToast(install_.title() + " is still installing", true);
        return;
    }
    offered_ = std::move(copies);
    panelUse_ = PanelUse::OfferInstall;
    shell_.launchPanel().open(game.title);
    if (offered_.size() == 1) {
        shell_.launchPanel().update("Not installed", std::nullopt, false);
        shell_.launchPanel().setHints({{"A", "Install"}, {"B", "Cancel"}});
        return;
    }
    // Two stores can install it: A is the first, X the second.
    shell_.launchPanel().update("Install from", std::nullopt, false);
    shell_.launchPanel().setHints({{"A", std::string{library::label(offered_[0].source)}},
                                   {"X", std::string{library::label(offered_[1].source)}},
                                   {"B", "Cancel"}});
}

void ShellApp::presentEula() {
    eulaWaiting_ = false;
    panelUse_ = PanelUse::Eula;
    const std::string title = install_.title();
    shell_.launchPanel().open(title);
    shell_.launchPanel().update("Installing " + title + " means accepting its licence agreement",
                                std::nullopt, false);
    shell_.launchPanel().setHints({{"A", "Accept"}, {"B", "Decline"}});
}

void ShellApp::serviceInstall() {
    if (eulaWaiting_ && panelUse_ != PanelUse::Launch) {
        presentEula();
    }
    const std::optional<InstallJob::Report> report = install_.take();
    if (!report) {
        return;
    }
    if (report->licence) {
        if (panelUse_ == PanelUse::Launch) {
            eulaWaiting_ = true;
        } else {
            presentEula();
        }
        return;
    }
    if (!report->finished) {
        if (panelUse_ == PanelUse::Install) {
            shell_.launchPanel().update(report->line, report->fraction);
        }
        return;
    }
    if (panelUse_ == PanelUse::Install || panelUse_ == PanelUse::Eula) {
        shell_.launchPanel().close();
        panelUse_ = PanelUse::None;
    }
    eulaWaiting_ = false;
    const std::string title = install_.title();
    if (report->failure.empty()) {
        reloadCatalog();
        shell_.setToast(title + " installed");
    } else {
        shell_.setToast("cannot install " + title + ": " + report->failure, true);
    }
}

void ShellApp::cancelLaunch() {
    {
        const std::lock_guard lock{launchMutex_};
        if (!launchRunning_) {
            return;
        }
    }
    lucent::info("launch", "cancelling the launch of {}", runningTitle_);
    handoff_.forceClose();
}

void ShellApp::actInGame(gamepad::Button button) {
    ui::GameMenu& menu = shell_.gameMenu();
    if (button == gamepad::Button::Guide) {
        setGameMenuOpen(!menu.isOpen());
        return;
    }
    if (!menu.isOpen()) {
        return;
    }
    switch (button) {
    case gamepad::Button::Up:
        menu.move(-1);
        break;
    case gamepad::Button::Down:
        menu.move(1);
        break;
    case gamepad::Button::B:
        setGameMenuOpen(false);
        break;
    case gamepad::Button::A:
        if (menu.selected() == ui::GameMenuAction::CloseGame) {
            lucent::info("launch", "closing {} from the Guide menu", runningTitle_);
            handoff_.forceClose();
        }
        setGameMenuOpen(false);
        break;
    default:
        break;
    }
}

void ShellApp::setGameMenuOpen(bool open) {
    ui::GameMenu& menu = shell_.gameMenu();
    if (open) {
        menu.open(runningTitle_);
    } else {
        menu.close();
    }
    if (padsHeld_) {
        pads_.setBlocked(open);
    }
    if (overlay_) {
        overlay_->setShown(open);
    } else if (open) {
        ClearWindowState(FLAG_WINDOW_HIDDEN);
    } else {
        SetWindowState(FLAG_WINDOW_HIDDEN);
    }
}

void ShellApp::showDetails() {
    if (const library::Game* game = shell_.focusedGame(); game != nullptr) {
        shell_.setToast(describe(*game));
    }
}

void ShellApp::refreshClock() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm parts{};
    localtime_r(&seconds, &parts);
    const auto millisecond = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() %
        1000);
    shell_.setClock(ui::ClockText::format(parts.tm_hour, parts.tm_min, config::read().clock24Hour));
    nextClockTick_ = std::chrono::steady_clock::now() +
                     ui::ClockText::untilNextMinute(parts.tm_sec, millisecond);
    // STOPGAP: the battery is re-read on the clock's minute tick because iideck has no
    // power_supply uevent listener standing in for iiSU's ACTION_BATTERY_CHANGED receiver.
    shell_.setBattery(battery_.read());
}

bool ShellApp::renderFrameToPng(std::string& png) {
    const RenderTexture target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
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

void ShellApp::requestCatalogReload(std::string toast) {
    requestToast(std::move(toast), false);
    reloadRequested_.store(true);
}

void ShellApp::requestGameRunning(bool running) {
    gameRunning_.store(running);
}

void ShellApp::requestLaunchProgress(const launch::LaunchProgress& progress) {
    const std::lock_guard lock{progressMutex_};
    pendingProgress_ = progress;
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
    } else if (const std::optional<library::Folder> folder = shell_.focusedFolder()) {
        next.focusedId =
            std::holds_alternative<library::Console>(*folder) ? "console:" + library::key(*folder) : library::key(*folder);
    }
    next.focusedTitle = shell_.focusedTitle();
    next.shelf = browser_.folder() ? library::key(*browser_.folder()) : "home";
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
    // What the loop has applied, not what was asked for, so a request that never reached the
    // loop cannot read as in game.
    next.inGame = shell_.inGame();
    next.gameMenuOpen = shell_.gameMenu().isOpen();
    next.steam = std::string{launch::name(steam_.state())};
    next.launchers = describe(launcherBadges(steam_.state(), steam_.downloads(), sources_));

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
    // An injected button is a tap: without its release a direction would repeat forever.
    for (const gamepad::Button button : queued) {
        handleEvents({gamepad::Event{
                          .kind = gamepad::Event::Kind::Button, .button = button, .pressed = true},
                      gamepad::Event{.kind = gamepad::Event::Kind::Button,
                                     .button = button,
                                     .pressed = false}});
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
    bool launching = false;
    {
        const std::lock_guard lock{launchMutex_};
        launching = launchRunning_;
    }
    if (padsHeld_ && !launching) {
        pads_.release();
        padsHeld_ = false;
    }
    std::optional<launch::LaunchProgress> progress;
    {
        const std::lock_guard lock{progressMutex_};
        progress = std::exchange(pendingProgress_, std::nullopt);
    }
    if (progress) {
        const bool measured = progress->stage == launch::LaunchProgress::Stage::Updating;
        shell_.launchPanel().update(launch::describe(*progress),
                                    measured ? std::optional{progress->fraction} : std::nullopt);
    }
    if (!launching && panelUse_ == PanelUse::Launch) {
        shell_.launchPanel().close();
        panelUse_ = PanelUse::None;
    }
    serviceInstall();
    if (reloadRequested_.exchange(false)) {
        reloadCatalog();
    }
    const bool running = gameRunning_.load();
    if (running != shell_.inGame()) {
        shell_.setInGame(running);
        shell_.gameMenu().close();
        if (panelUse_ == PanelUse::Launch) {
            shell_.launchPanel().close();
            panelUse_ = PanelUse::None;
        }
        // raylib has no ShowWindow or HideWindow: hiding is a window state flag, and showing is
        // clearing it.
        if (overlay_) {
            if (running) {
                overlay_->enter();
            } else {
                overlay_->leave();
            }
        } else if (running) {
            SetWindowState(FLAG_WINDOW_HIDDEN);
        } else {
            ClearWindowState(FLAG_WINDOW_HIDDEN);
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
    // Transparent, so the Guide menu can draw over a game with the game showing through. No
    // MSAA: with it, Gamescope's Xwayland gave a 24-bit window, which it composites as opaque.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TRANSPARENT);
    InitWindow(settings_.width, settings_.height, "iideck");
    SetWindowMinSize(960, 600);
    if (config::read().insideGamescope) {
        // Gamescope composites a window as the overlay only when it spans the whole screen,
        // which also draws the home screen at the output's own resolution.
        const int monitor = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        shell_.setSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        overlay_ = std::make_unique<session::GamescopeOverlay>(
            *static_cast<const unsigned long*>(GetWindowHandle()));
        gameKeys_ = std::make_unique<session::GameKeys>();
    }
    // Textures need a GL context, so artwork is loaded only once the window is up.
    shell_.loadArtwork();
    lucent::info("ui", "artwork loaded for {} of {} tiles", shell_.loadedArtwork(),
                 shell_.tiles().size());
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    // The control channel is part of the product, not a debug flag: it is how an
    // automated run drives the shell without a controller.
    if (settings_.controlChannel) {
        control_ = std::make_unique<ControlChannel>(*this, signIn_, settings_.controlPort);
        control_->start();
    }

    // Steam comes up in the background while the shell is already usable. A machine
    // without a Steam install has nothing to start, and shows no Steam icon.
    const config::Config& config = config::read();
    if (!library::steam::Library::discover(config.home, config.steamRoots).roots().empty()) {
        steam_.start();
    }

    while (!closeRequested_.load() && !WindowShouldClose()) {
        // A resize changes the framebuffer, so the layout has to be recomputed
        // before anything is drawn into it. Checked every frame because there is
        // no resize callback worth relying on across platforms.
        if (IsWindowResized()) {
            shell_.setSize(GetScreenWidth(), GetScreenHeight());
        }

        handleEvents(pads_.takeEvents());
        handleKeyboard();
        handleGameKeys();
        if (const auto direction = repeat_.poll(std::chrono::steady_clock::now())) {
            actOn(*direction);
        }

        serviceControlRequests();
        serviceRequests();
        serviceArtwork();
        shell_.setLaunchers(launcherBadges(steam_.state(), steam_.downloads(), sources_));
        // iiSU pl3.q: Home has no title; inside a folder the pill names the focused game.
        shell_.setTitle(browser_.folder() ? shell_.focusedTitle() : std::string{});
        shell_.tick(std::chrono::steady_clock::now());
        publishSnapshot();
        shell_.draw();

        // iiSU k42: the clock re-renders on the minute boundary.
        if (std::chrono::steady_clock::now() >= nextClockTick_) {
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