// retro_session — the shell's side of opensu-retro's control channel: the control file each launch
// names in OPENSU_RETRO_CONTROL and the settings file beside it, the port read from the control
// file once the runner announces one, the requests the quick menu and the game chords send, and the
// load failure a runner leaves there.
#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

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

    /// The running game's state when it runs on opensu-retro and answers; nothing otherwise.
    [[nodiscard]] std::optional<Status> status();
    /// Each asks the runner and answers empty when it was done, else why not.
    [[nodiscard]] std::string saveState();
    [[nodiscard]] std::string loadState();
    [[nodiscard]] std::string reset();

    /// What the quick menu's game rows and the game chords ask for.
    enum class Command : std::uint8_t { SaveState, LoadState, Reset };
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
    [[nodiscard]] std::string post(std::string_view target);

    std::filesystem::path directory_;
    std::mutex mutex_;
    std::filesystem::path file_;
    std::optional<std::uint16_t> port_;
    unsigned launches_{0};
};

} // namespace opensu::app
