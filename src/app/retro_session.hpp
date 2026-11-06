// retro_session — the shell's side of opensu-retro's control channel: the control file each launch
// names in OPENSU_RETRO_CONTROL and the settings file beside it, the port read from the control
// file once the runner announces one, the requests the Guide menu and the game chords send, the
// Rotate screen choice and the load failure a runner leaves there.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "settings/core_option_settings.hpp"
#include "settings/video_settings.hpp"

namespace opensu::app {

class RetroSession {
  public:
    /// The variable naming the control file in a game's environment.
    static constexpr std::string_view variable = "OPENSU_RETRO_CONTROL";

    /// What the runner reports of the game it runs.
    struct Status {
        std::string core;
        std::string rom;
        std::uint64_t frame{0};
        bool hasState{false};
        /// The machine's panel (`touch::panelText`); empty for no arcade game.
        std::string panel;
        /// The core's picture in pixels, 0 by 0 when it is turned, and its shape as shown.
        unsigned pictureWidth{0};
        unsigned pictureHeight{0};
        float pictureAspect{0.0F};
        /// A frame has reached the screen.
        bool shown{false};
        /// When the state the game resumed from was taken; nothing when it did not resume.
        std::optional<std::chrono::system_clock::time_point> resumedFrom;
        /// Why the game's resume state was not loaded; empty when none was refused.
        std::string resumeRefused;
    };

    /// Control files go in `directory`, `retro-<shell pid>-<launch>.json`.
    explicit RetroSession(std::filesystem::path directory);

    /// A new launch's control file, as the `NAME=value` entry for the game's environment. Any
    /// thread.
    [[nodiscard]] std::string begin();
    /// The launch begun last's settings file for `opensu-retro --settings`, beside its control
    /// file. Any thread.
    [[nodiscard]] std::filesystem::path settingsFile();
    /// The launch is over: the load failure the runner left, empty when none. Its files are
    /// removed. Any thread.
    [[nodiscard]] std::string end();

    /// What the player last chose with Rotate screen in the launch running or just over; nothing
    /// when they did not press it. Any thread.
    [[nodiscard]] std::optional<bool> screenTurned();

    /// The running game's state when it runs on opensu-retro and answers; nothing otherwise.
    [[nodiscard]] std::optional<Status> status();
    /// One of the running core's options: how the core defines it and its value now.
    struct CoreOption {
        settings::CoreOptionDefinition definition;
        std::string value;
    };
    /// The UDP port the running game's netplay session bound; nothing until it says.
    [[nodiscard]] std::optional<std::uint16_t> netplayPort();
    /// The last frame the game showed, as PNG; nothing when it does not answer.
    [[nodiscard]] std::optional<std::string> frame();
    /// The running core's options; none when it does not answer.
    [[nodiscard]] std::vector<CoreOption> options();
    [[nodiscard]] std::string setOption(const std::string& key, const std::string& value);
    /// Each asks the runner and answers empty when it was done, else why not.
    [[nodiscard]] std::string saveState();
    [[nodiscard]] std::string loadState();
    /// Asks the window to take the picture's own size; the reason when refused, else empty.
    [[nodiscard]] std::string native();
    [[nodiscard]] std::string reset();
    /// Save and close's first half: the runner keeps a resume state, writes the game's save and
    /// runs no more frames, waiting to be closed.
    [[nodiscard]] std::string suspend();
    /// Stops or restarts the game's frames and sound, its picture staying; a netplay game refuses.
    [[nodiscard]] std::string hold(bool held);
    /// Shows the game under `video` from its next frame.
    [[nodiscard]] std::string setVideo(const settings::VideoSettings& video);

    /// Posts `body` to `route`, for the input of a game in the shell's window; one it does not
    /// take is logged.
    void relay(const std::string& route, const std::string& body);

    /// What the Guide menu's game rows and the game chords ask for.
    enum class Command : std::uint8_t { SaveState, LoadState, Reset, TurnScreen };
    /// How a command went, as the toast says it.
    struct Outcome {
        std::string text;
        bool error{false};
    };
    /// Asks the runner for `command`: "State saved" and the like, or why it was not done.
    [[nodiscard]] Outcome perform(Command command);

  private:
    /// The port the current control file announces, read once it does.
    [[nodiscard]] std::optional<std::uint16_t> port();
    [[nodiscard]] std::string post(std::string_view target, const std::string& body = {});
    [[nodiscard]] std::optional<std::string> get(std::string_view target);

    std::filesystem::path directory_;
    std::mutex mutex_;
    std::filesystem::path file_;
    std::optional<std::uint16_t> port_;
    /// The choice of the launch just over, until it is taken.
    std::optional<bool> turned_;
    unsigned launches_{0};
};

} // namespace opensu::app
