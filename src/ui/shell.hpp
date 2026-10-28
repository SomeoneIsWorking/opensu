// ui — the drawn shell.
//
// Composes what the window shows: the interface toolkit's navigator, whose root is Home and whose
// screens open over it, the home grid's tiles, the chrome around them (the top bar, the dock, the
// docked keyboard and the mapping editor, each a chrome screen of the navigator), the HUD's hints
// and toasts, the volume display and the chord list. It lays them out each tick and draws them in
// two passes, so the dock's glass blurs the scene under it.
#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/image_loader.hpp"
#include "gpu2d/render_target.hpp"
#include "gpu2d/surface.hpp"

#include "chord_hint.hpp"
#include "chord_hint_painter.hpp"
#include "config/config.hpp"
#include "corner_hints.hpp"
#include "home_dock.hpp"
#include "home_grid.hpp"
#include "hud.hpp"
#include "input/chord_set.hpp"
#include "input/prompts.hpp"
#include "interface_text.hpp"
#include "keyboard_chrome.hpp"
#include "library/sections.hpp"
#include "mapping_chrome.hpp"
#include "mapping_page.hpp"
#include "on_screen_keyboard.hpp"
#include "paint.hpp"
#include "screen_stack.hpp"
#include "top_bar_chrome.hpp"
#include "vector_icon.hpp"
#include "volume_osd.hpp"
#include "volume_osd_painter.hpp"

namespace opensu::ui {

class Shell {
  public:
    using Clock = std::chrono::steady_clock;

    /// `assetsDir` holds the face and the icons the shell draws.
    Shell(int width, int height, config::HomeMode mode, const std::filesystem::path& assetsDir);

    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;
    Shell(Shell&&) = delete;
    Shell& operator=(Shell&&) = delete;
    ~Shell() = default;

    /// Reports a new window size and re-flows the grid.
    void setSize(int width, int height);

    /// Asks for the grid's artwork (`HomeGrid::requestArtwork`) and lands up to `bytes` of decoded
    /// images (the grid's, the screens' pictures and the icons) as textures. Once a frame, drawn
    /// or not.
    void loadArtwork(std::size_t bytes = uploadBytesPerFrame);
    /// Asks for the grid's artwork and waits until every image asked for has landed; for a still
    /// render.
    void loadArtworkNow();
    /// Waits for the icons the last draw asked for and lands them; true when there were any, so
    /// a still frame is drawn again with them.
    bool settleIcons();

    /// Draws on `device` from here on: loads the face and makes every texture there. Once, when
    /// the window is up; throws gpu2d::GpuError when no face loads.
    void attach(SDL_GPUDevice& device);

    /// Pixels landed as textures per frame (at least one image), so a screenful of new covers
    /// spreads over a few frames.
    static constexpr std::size_t uploadBytesPerFrame = std::size_t{4} << 20U;

    /// Makes `section` the one shown, which sets its grid and where the dock stands. The shelf is
    /// the caller's to replace after. `revealDock` slides the dock in for a moment (iiSU
    /// `jk2.java:937`), as an L1 or R1 press does.
    void setSection(library::Section section, bool revealDock);
    /// Draws everything at `factor` of its size; the layouts are drawn at 1.
    void setUiScale(float factor);
    /// The on-screen keyboard style every field opens with from now on.
    void setKeyboardStyle(input::KeyboardStyle style) noexcept {
        keyboard_.setStyle(style);
    }
    /// Shows the volume display with `level`.
    void showVolume(const VolumeLevel& level) {
        volumeOsd_.show(level, now_);
    }
    [[nodiscard]] const VolumeOsd& volumeOsd() const noexcept {
        return volumeOsd_;
    }
    /// Shows the chords of `modifier`, held long, or hides them for nothing.
    void followChordHint(std::optional<gamepad::Button> modifier) {
        chordHint_.follow(modifier, chords_, now_);
    }
    [[nodiscard]] const ChordHint& chordHint() const noexcept {
        return chordHint_;
    }
    /// Whether the chord list is drawn, fading out included.
    [[nodiscard]] bool chordHintVisible() const noexcept {
        return chordHint_.visible(now_);
    }

    /// The interface toolkit's screens: Home at the bottom, then the Guide menu, Settings, a
    /// game's details, the tile menu and what opens over them.
    [[nodiscard]] ScreenStack& screens() noexcept {
        return screens_;
    }
    [[nodiscard]] const ScreenStack& screens() const noexcept {
        return screens_;
    }
    /// Home's tiles.
    [[nodiscard]] HomeGrid& grid() noexcept {
        return grid_;
    }
    [[nodiscard]] const HomeGrid& grid() const noexcept {
        return grid_;
    }
    [[nodiscard]] TopBarChrome& topBar() noexcept {
        return topBar_;
    }
    [[nodiscard]] HomeDock& dock() noexcept {
        return dock_;
    }
    [[nodiscard]] KeyboardChrome& keyboardChrome() noexcept {
        return keyboardChrome_;
    }
    /// The on-screen keyboard docked along the bottom edge, which types into the focused field
    /// and takes the place of the nav dock and the corner hints while it is up.
    [[nodiscard]] OnScreenKeyboard& keyboard() noexcept {
        return keyboard_;
    }
    [[nodiscard]] const OnScreenKeyboard& keyboard() const noexcept {
        return keyboard_;
    }

    /// Which device the prompts name. The caller feeds it; the painters read it.
    [[nodiscard]] input::LastDevice& inputDevice() noexcept {
        return prompts_.device;
    }
    /// The shortcut table the prompts name keys from; the app rebinds it.
    [[nodiscard]] input::Shortcuts& shortcuts() noexcept {
        return prompts_.shortcuts;
    }
    /// The chords in force: the shortcut table's and the running game's.
    [[nodiscard]] input::ChordSet& chords() noexcept {
        return chords_;
    }
    /// The button mapping editor, a card over the details page or the Settings screen.
    [[nodiscard]] MappingPage& mappingPage() noexcept {
        return mapping_;
    }
    [[nodiscard]] const MappingPage& mappingPage() const noexcept {
        return mapping_;
    }

    /// Advances motion and the toast to `now` and lays the navigator and its chrome out.
    void tick(Clock::time_point now);

    /// Advances past every one-shot motion, for a still frame.
    void settle();

    /// Records one frame into `target` on `canvas`: the home screen, or while a game runs, only the
    /// Guide menus and the pages they open over a transparent frame. The caller submits it.
    void draw(Canvas& canvas, const gpu2d::Surface& target);

    /// Whether a game is running, which turns the home screen into the in-game overlay.
    void setInGame(bool inGame) noexcept {
        inGame_ = inGame;
    }
    [[nodiscard]] bool inGame() const noexcept {
        return inGame_;
    }

    [[nodiscard]] const std::string& status() const noexcept {
        return hud_.status();
    }
    [[nodiscard]] const std::string& toast() const noexcept {
        return hud_.toast();
    }
    [[nodiscard]] bool toastIsError() const noexcept {
        return hud_.toastIsError();
    }

    void setStatus(std::string text) {
        hud_.setStatus(std::move(text));
    }
    void setClock(std::string text) {
        hud_.setClock(std::move(text));
    }
    void setToast(std::string text, bool isError = false) {
        hud_.setToast(std::move(text), isError, now_);
    }
    void setLaunchers(std::vector<LauncherBadge> launchers) {
        hud_.setLaunchers(std::move(launchers));
    }
    void setHints(const HintContext& hints) noexcept {
        hud_.setHints(hints);
    }
    void setTitle(std::string title) {
        hud_.setTitle(std::move(title));
    }
    /// The breadcrumb trail the top bar shows.
    void setTrail(Trail trail) {
        hud_.setTrail(std::move(trail));
    }
    void setBattery(std::optional<device::BatteryStatus> battery) noexcept {
        hud_.setBattery(battery);
    }

  private:
    /// Pixels per dp.
    [[nodiscard]] float dp() const noexcept;
    /// Logs what failed to load among `uploads`.
    static void land(const gpu2d::ImageUploads& uploads);
    /// Gives the grid the window and the room the chrome keeps.
    void frameGrid();
    /// The held modifier's chords, while shown or fading out.
    void drawChordHint(Canvas& canvas, Point size);
    /// The scene's target, as big as the window.
    [[nodiscard]] gpu2d::RenderTarget& sceneTarget();
    [[nodiscard]] SDL_GPUDevice& device() const;

    /// Before every owner that asks it for images.
    gpu2d::ImageLoader images_;
    /// Before the painters that draw with it.
    InterfaceText text_;
    IconAtlas icons_;
    /// Before the painters that read it.
    input::Prompts prompts_;
    input::ChordSet chords_{prompts_.shortcuts};
    float uiScale_{1.0f};
    VolumeOsd volumeOsd_;
    ChordHint chordHint_;
    /// Before the screens, whose prompts give way to it.
    OnScreenKeyboard keyboard_;
    ScreenStack screens_{prompts_, text_, keyboard_, images_};
    MappingPage mapping_;
    Hud hud_{prompts_, text_, icons_};
    HomeGrid grid_;
    HomeDock dock_{prompts_, text_, icons_};
    TopBarChrome topBar_{hud_};
    KeyboardChrome keyboardChrome_{keyboard_, prompts_, text_};
    MappingChrome mappingChrome_{mapping_, prompts_, text_};

    Clock::time_point now_{Clock::now()};

    VolumeOsdPainter volumeOsdPainter_{text_};
    ChordHintPainter chordHintPainter_{prompts_, text_};
    /// The frame up to the dock, which the dock's glass blurs.
    std::optional<gpu2d::RenderTarget> scene_;
    SDL_GPUDevice* device_{};

    int width_{};
    int height_{};
    bool inGame_{false};
};

} // namespace opensu::ui
