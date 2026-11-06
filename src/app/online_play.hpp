// online_play — playing a ROM with a friend over the internet, on the shell's loop. Play online on
// a game says what the friend will see, then makes a code (`online::Meeting`); Settings › Play
// online takes a friend's code and, when the host cannot be reached with that alone, shows a reply
// code to send back. What it finds it puts on `OnlineScreen`; once both sides agree the session
// starts through `NetplayRun`, and this holds the router's port mapping until the game ends.
#pragma once

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "keyboard_pad.hpp"
#include "library/game.hpp"
#include "netplay_run.hpp"
#include "online/meeting.hpp"
#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class OnlinePlay {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The games of this catalog.
        std::function<const std::vector<library::Game>&()> games;
        /// Puts `text` on the system clipboard.
        std::function<void(const std::string& text)> copy;
    };

    /// What `OnlineScreen` shows.
    struct Page {
        std::string title;
        std::string note;
        /// The code to send; empty for none.
        std::string code;
        /// Whether the host may paste a reply.
        bool replyField{false};
        /// The meeting is over: Close, not Cancel.
        bool over{false};
    };

    OnlinePlay(gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds, NetplayRun& run,
               KeyboardPad& keyboard, Preferences& preferences, Hooks hooks);

    /// The Settings category's id, which its rows' ids start with.
    static constexpr std::string_view categoryId = "online";
    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);

    /// Offers `game`, once the player has read who will see what.
    void host(const library::Game& game);
    /// Takes a friend's code, once the player has read the same.
    void join();
    /// Once a frame.
    void service();
    /// The game ended: the router's mapping goes.
    void ended();

    [[nodiscard]] Page shown() const;
    [[nodiscard]] const std::string& replyDraft() const noexcept {
        return replyDraft_;
    }
    void editReply(std::string text) {
        replyDraft_ = std::move(text);
    }
    /// The player's reply field is done.
    void submitReply();
    void copyCode();
    /// Cancel, or Close once it is over.
    void close();

    /// Where it stands, for the control channel.
    [[nodiscard]] std::string glance() const;
    /// The code shown now.
    [[nodiscard]] std::string code() const;

  private:
    void startHost(const library::Game& game);
    void askCode();
    void closeCode();
    void submitCode();
    void openScreen();
    void closeScreen();
    void question(const library::Game& game);
    void closeQuestion();
    void lookFor(const lan::wire::NetplayAsk& asked);
    void serviceLookup();
    void serviceHash();
    void begin(online::Agreement& agreement, const library::Game& game);
    [[nodiscard]] online::Network network() const;

    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    NetplayRun& run_;
    KeyboardPad& keyboard_;
    Preferences& preferences_;
    Hooks hooks_;
    /// The ROM's hash, off the loop, before the host's meeting starts.
    std::optional<std::future<std::string>> hashing_;
    std::unique_ptr<online::Meeting> meeting_;
    /// The game of this session: the host's, or the friend's own copy once found.
    std::optional<library::Game> game_;
    bool hosting_{false};
    bool answered_{false};
    std::string questionTitle_;
    std::optional<std::future<std::optional<library::Game>>> lookup_;
    std::string codeDraft_;
    std::string replyDraft_;
    std::string replyProblem_;
    std::unique_ptr<online::PortLease> lease_;
    online::Meeting::View view_;
};

} // namespace opensu::app
