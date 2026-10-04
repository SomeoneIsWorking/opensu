// app — composition. Owns the catalog, the controller reader and the drawn
// shell, and is the only place the two halves meet.
#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "raylib.h"

#include "gamepad/reader.hpp"
#include "launch/handoff.hpp"
#include "library/catalog.hpp"
#include "ui/shell.hpp"

namespace iideck::app {

/// What the shell needs from the host, so the shell can be drawn without a
/// running store client.
struct Settings {
    int width{1280};
    int height{800};
    /// Explicit Steam install roots; discovered when empty.
    std::vector<std::string> steamRoots;
    /// Directories scanned for emulator ROMs.
    std::vector<std::string> romRoots;
    /// Emulator commands as "SYSTEM=program|arg|arg;SYSTEM2=program".
    std::string emulatorCommands;
};

/// The running shell.
class ShellApp {
public:
    explicit ShellApp(Settings settings);

    /// Loads the library, then runs the window loop until it closes.
    int run();

    /// Renders one frame offscreen and writes it to `path`, without opening a
    /// window. This is what makes the layout checkable from a test.
    bool renderToFile(const std::string& path);

    /// The catalog as last read.
    [[nodiscard]] const std::vector<library::Game>& games() const noexcept { return games_; }

private:
    /// Flips an image in place, for the render texture's bottom-up origin.
    static void flipVertical(Image& image);

    void reloadCatalog();
    void handleEvents(const std::vector<gamepad::Event>& events);
    void actOn(gamepad::Button button);
    void launchFocused();
    void showDetails();
    void refreshClock();
    void pushCatalogToShell();

    Settings settings_;
    library::Catalog catalog_;
    ui::Shell shell_;
    gamepad::Reader pad_;

    std::vector<library::Game> games_;
    /// Guards the handoff thread, which touches the window.
    std::mutex launchMutex_;
    bool launchRunning_{false};
    bool closeRequested_{false};
};

} // namespace iideck::app