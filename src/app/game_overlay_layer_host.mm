// The macOS overlay. A Wine game whose winemac.drv says hello on the launch's `EmbedSocket` is
// drawn inside the shell's window: each window it reports is a CALayerHost showing that window's
// CAContext, in a view under SDL's Metal view, so the window server composites the game's own
// frames under openSU's interface with no copy and no Screen Recording permission. While the game
// has the input (no menu or page up) the keyboard, mouse and wheel over it go to the socket instead
// of the shell, and the shortcuts that work in-game are watched on the way. Any other game (a
// native one, a runner without the socket, or a Wine game that shows no window within 10 s) gets
// the raised window: the shell lowered behind it and raised over it, the game showing through the
// translucent window. docs/plans/mac-wine-embed.md.
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game_overlay.hpp"
#include "input/key_names.hpp"
#include "input/shortcut_chord.hpp"
#include "lucent/log.h"

namespace opensu::app {
namespace {

using Clock = std::chrono::steady_clock;

/// How long a Wine game that said hello has to report a window once the launch sees it running.
constexpr auto windowWait = std::chrono::seconds{10};

/// What the keys from kVK_ANSI_A (0) to kVK_ANSI_Grave (50) are on the US layout; a NUL where
/// none is a shortcut key.
constexpr std::string_view usKeys{"ASDFHGZXCV\0BQWERYT123465=97-80]OU[IP\0LJ'K;\\,/NM.\0 `", 51};

/// The shell's key code (`input::keys`) for a Mac virtual key code, or nothing.
std::optional<int> keyOf(unsigned short code) {
    namespace keys = input::keys;
    if (code < usKeys.size() && usKeys[code] != '\0') {
        return static_cast<unsigned char>(usKeys[code]);
    }
    switch (code) {
    case 36:
        return keys::Enter;
    case 48:
        return keys::Tab;
    case 51:
        return keys::Backspace;
    case 53:
        return keys::Escape;
    case 114:
        return keys::Insert;
    case 123:
        return keys::Left;
    case 124:
        return keys::Right;
    case 125:
        return keys::Down;
    case 126:
        return keys::Up;
    case 116:
        return keys::PageUp;
    case 121:
        return keys::PageDown;
    case 115:
        return keys::Home;
    case 119:
        return keys::End;
    case 76:
        return keys::KeypadEnter;
    case 56:
        return keys::LeftShift;
    case 59:
        return keys::LeftControl;
    case 58:
        return keys::LeftAlt;
    case 55:
        return keys::LeftSuper;
    case 60:
        return keys::RightShift;
    case 62:
        return keys::RightControl;
    case 61:
        return keys::RightAlt;
    case 54:
        return keys::RightSuper;
    default:
        break;
    }
    // F1 to F12, in Carbon's order.
    static constexpr unsigned short functionKeys[] = {122, 120, 99,  118, 96,  97,
                                                      98,  100, 101, 109, 103, 111};
    for (int index = 0; index < 12; ++index) {
        if (functionKeys[index] == code) {
            return keys::F1 + index;
        }
    }
    return std::nullopt;
}

/// The modifier flag a modifier key's code holds while it is down, or 0 for another key.
NSEventModifierFlags flagOf(unsigned short code) {
    switch (code) {
    case 56:
    case 60:
        return NSEventModifierFlagShift;
    case 59:
    case 62:
        return NSEventModifierFlagControl;
    case 58:
    case 61:
        return NSEventModifierFlagOption;
    case 55:
    case 54:
        return NSEventModifierFlagCommand;
    case 57:
        return NSEventModifierFlagCapsLock;
    default:
        return 0;
    }
}

class LayerHostOverlay final : public GameOverlay {
  public:
    LayerHostOverlay(ShellWindow& window, EmbedSocket& embed)
        : window_{window}, embed_{embed}, nsWindow_{(__bridge NSWindow*)window.cocoaWindow()},
          hostClass_{NSClassFromString(@"CALayerHost")} {
        if (nsWindow_ == nil || hostClass_ == nil) {
            lucent::warn("embed", "Wine games are shown in their own windows: {}",
                         nsWindow_ == nil ? "the shell's window has no NSWindow"
                                          : "this macOS has no CALayerHost");
            return;
        }
        NSView* content = nsWindow_.contentView;
        gameView_ = [[NSView alloc] initWithFrame:content.bounds];
        gameView_.wantsLayer = YES;
        gameView_.layer.backgroundColor = NSColor.blackColor.CGColor;
        gameView_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        gameView_.hidden = YES;
        // Under SDL's Metal view, a sibling in the content view, so the interface draws over it.
        [content addSubview:gameView_ positioned:NSWindowBelow relativeTo:nil];
        const NSEventMask mask =
            NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged |
            NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskRightMouseDown |
            NSEventMaskRightMouseUp | NSEventMaskOtherMouseDown | NSEventMaskOtherMouseUp |
            NSEventMaskMouseMoved | NSEventMaskLeftMouseDragged | NSEventMaskRightMouseDragged |
            NSEventMaskOtherMouseDragged | NSEventMaskScrollWheel;
        monitor_ = [NSEvent addLocalMonitorForEventsMatchingMask:mask
                                                         handler:^NSEvent*(NSEvent* event) {
                                                           return forward(event) ? nil : event;
                                                         }];
    }

    ~LayerHostOverlay() override {
        if (monitor_ != nil) {
            [NSEvent removeMonitor:monitor_];
        }
        showCursor(true);
        [gameView_ removeFromSuperview];
    }
    LayerHostOverlay(const LayerHostOverlay&) = delete;
    LayerHostOverlay& operator=(const LayerHostOverlay&) = delete;
    LayerHostOverlay(LayerHostOverlay&&) = delete;
    LayerHostOverlay& operator=(LayerHostOverlay&&) = delete;

    [[nodiscard]] bool seesGame() const override {
        return state_ == State::Embedded || window_.translucent();
    }

    [[nodiscard]] bool embedsWineGames() const override {
        return gameView_ != nil;
    }

    void enter() override {
        if (gameView_ != nil && embed_.isOpen() && embed_.gameSaidHello()) {
            state_ = State::Embedded;
            deadline_ = Clock::now() + windowWait;
            gameView_.hidden = NO;
            layout();
            embed_.setFocus(shown_ != display::OverlayMode::Interactive);
            // The game's own window may have taken the front; the shell's is the one it plays in.
            window_.raise();
            if (embed_.sawWindow()) {
                lucent::info("embed", "the game is shown inside openSU's window");
            } else {
                lucent::info("embed", "the game said hello; its window shows inside openSU's "
                                      "window when it sends one");
            }
            return;
        }
        if (embed_.isOpen()) {
            lucent::info("embed", "no Wine game said hello ({}); it plays in its own window",
                         hellosText());
        }
        state_ = State::Raised;
        window_.lower();
    }

    void show(display::OverlayMode mode) override {
        shown_ = mode;
        if (state_ == State::Embedded) {
            embed_.setFocus(mode != display::OverlayMode::Interactive);
            if (mode == display::OverlayMode::Interactive) {
                showCursor(true);
            }
            return;
        }
        if (mode == display::OverlayMode::Interactive) {
            window_.raise();
        } else {
            window_.lower();
        }
    }

    void leave() override {
        const bool raised = state_ != State::Embedded;
        state_ = State::Idle;
        shown_ = display::OverlayMode::Hidden;
        clearLayers();
        showCursor(true);
        if (gameView_ != nil) {
            gameView_.hidden = YES;
        }
        if (raised) {
            window_.raise();
        }
    }

    std::vector<input::Action> gameKeys(const input::Shortcuts& shortcuts) override {
        std::vector<input::Action> actions;
        for (const input::Combo& combo : pressed_) {
            if (const std::optional<input::Action> action = shortcuts.actionFor(combo);
                action && input::worksInGame(*action)) {
                actions.push_back(*action);
            }
        }
        pressed_.clear();
        return actions;
    }

    void service() override {
        if (gameView_ == nil || !embed_.isOpen()) {
            return;
        }
        const NSSize size = gameView_.bounds.size;
        embed_.setScreen(
            {.width = size.width, .height = size.height, .scale = nsWindow_.backingScaleFactor});
        const bool changed = embed_.poll();
        if (state_ != State::Embedded) {
            return;
        }
        if (changed || !NSEqualSizes(size, laidOut_)) {
            layout();
        }
        if (!embed_.sawWindow() && Clock::now() >= deadline_) {
            lucent::warn("embed",
                         "the game sent no window in {} s ({}); launch: {}; it plays in its own "
                         "window",
                         windowWait.count(), hellosText(), embed_.launchTree());
            leave();
            state_ = State::Raised;
            window_.lower();
            return;
        }
        followCursor();
    }

  private:
    enum class State : std::uint8_t { Idle, Embedded, Raised };

    /// Where a window shows in the game view, bottom-left origin, and its points' scale.
    struct Placed {
        EmbedSocket::Window window;
        NSRect rect;
        double scale;
    };

    /// Whether the game has the input now.
    [[nodiscard]] bool forwarding() const {
        return state_ == State::Embedded && shown_ != display::OverlayMode::Interactive;
    }

    std::string hellosText() const {
        std::string text;
        for (const EmbedSocket::Hello& hello : embed_.hellos()) {
            text += (text.empty() ? "" : ", ") + hello.program + " " + std::to_string(hello.pid);
        }
        return text.empty() ? "none" : "hello from " + text;
    }

    /// The game's main window fills the view, letterboxed, as a Gamescope game fills its output;
    /// the others keep their places beside it at its scale.
    void layout() {
        const NSSize view = gameView_.bounds.size;
        laidOut_ = view;
        placed_.clear();
        const std::vector<EmbedSocket::Window>& windows = embed_.windows();
        const EmbedSocket::Window* main = nullptr;
        for (const EmbedSocket::Window& window : windows) {
            const bool better = main == nullptr ||
                                (window.kind == "normal" && main->kind != "normal") ||
                                (window.kind == main->kind &&
                                 window.width * window.height > main->width * main->height);
            if (better && window.width > 0 && window.height > 0) {
                main = &window;
            }
        }
        if (main != nullptr) {
            const double scale = std::min(view.width / main->width, view.height / main->height);
            const double left = (view.width - main->width * scale) / 2 - main->x * scale;
            const double top = (view.height - main->height * scale) / 2 - main->y * scale;
            for (const EmbedSocket::Window& window : windows) {
                const double height = window.height * scale;
                placed_.push_back({window,
                                   NSMakeRect(left + window.x * scale,
                                              view.height - (top + window.y * scale) - height,
                                              window.width * scale, height),
                                   scale});
            }
        }
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        std::map<std::uint64_t, CALayer*> kept;
        for (const Placed& place : placed_) {
            CALayer* host = nil;
            if (const auto found = hosts_.find(place.window.id);
                found != hosts_.end() && [[found->second valueForKey:@"contextId"]
                                             unsignedIntValue] == place.window.context) {
                host = found->second;
                hosts_.erase(found);
            } else {
                host = [hostClass_ layer];
                [host setValue:@(place.window.context) forKey:@"contextId"];
                host.anchorPoint = CGPointZero;
                host.masksToBounds = YES;
            }
            host.bounds = CGRectMake(0, 0, place.window.width, place.window.height);
            host.position = place.rect.origin;
            host.transform = CATransform3DMakeScale(place.scale, place.scale, 1);
            [host removeFromSuperlayer];
            [gameView_.layer addSublayer:host];
            kept[place.window.id] = host;
        }
        for (auto& [id, host] : hosts_) {
            [host removeFromSuperlayer];
        }
        hosts_ = std::move(kept);
        [CATransaction commit];
    }

    void clearLayers() {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        for (auto& [id, host] : hosts_) {
            [host removeFromSuperlayer];
        }
        [CATransaction commit];
        hosts_.clear();
        placed_.clear();
    }

    /// The topmost window under `point` in the game view, or null.
    const Placed* windowAt(NSPoint point) const {
        for (auto place = placed_.rbegin(); place != placed_.rend(); ++place) {
            if (NSPointInRect(point, place->rect)) {
                return &*place;
            }
        }
        return nullptr;
    }

    /// Sends `event` to the game when it has the input; true when it was the game's.
    bool forward(NSEvent* event) {
        if (!forwarding() || event.window != nsWindow_) {
            return false;
        }
        switch (event.type) {
        case NSEventTypeKeyDown:
        case NSEventTypeKeyUp: {
            // ⌘ keys stay the Mac's (⌘Q, ⌘H, ⌘Tab); a Windows game has no use for them.
            if ((event.modifierFlags & NSEventModifierFlagCommand) != 0) {
                return false;
            }
            const bool down = event.type == NSEventTypeKeyDown;
            embed_.key({.down = down,
                        .keycode = event.keyCode,
                        .chars = event.characters.UTF8String ?: "",
                        .modifiers = event.modifierFlags,
                        .repeat = down && event.isARepeat});
            watch(event.keyCode, down);
            return true;
        }
        case NSEventTypeFlagsChanged: {
            const NSEventModifierFlags flag = flagOf(event.keyCode);
            if (flag == 0) {
                return false;
            }
            const bool down = (event.modifierFlags & flag) != 0;
            embed_.key({.down = down,
                        .keycode = event.keyCode,
                        .chars = {},
                        .modifiers = event.modifierFlags,
                        .repeat = false});
            watch(event.keyCode, down);
            return true;
        }
        case NSEventTypeScrollWheel: {
            const NSPoint point = [gameView_ convertPoint:event.locationInWindow fromView:nil];
            const Placed* place = windowAt(point);
            if (place == nullptr) {
                return false;
            }
            embed_.scroll(place->window, event.scrollingDeltaX, event.scrollingDeltaY,
                          event.hasPreciseScrollingDeltas);
            return true;
        }
        default: {
            const NSPoint point = [gameView_ convertPoint:event.locationInWindow fromView:nil];
            const Placed* place = windowAt(point);
            if (place == nullptr) {
                return false;
            }
            const double x = (point.x - place->rect.origin.x) / place->scale;
            const double y = (NSMaxY(place->rect) - point.y) / place->scale;
            // NSEvent's bits are left 1, right 2, other 4, as the socket's.
            embed_.mouse(place->window, x, y,
                         static_cast<unsigned>(NSEvent.pressedMouseButtons) & 7U);
            if (embed_.cursorHidden() && (event.deltaX != 0 || event.deltaY != 0)) {
                embed_.relative(event.deltaX, event.deltaY);
            }
            return true;
        }
        }
    }

    /// Keeps a forwarded key for the in-game shortcuts.
    void watch(unsigned short code, bool down) {
        if (const std::optional<int> key = keyOf(code)) {
            if (const std::optional<input::Combo> combo = chord_.key(*key, down)) {
                pressed_.push_back(*combo);
            }
        }
    }

    /// Hides the pointer over the game while the game hides its own and has the input.
    void followCursor() {
        bool hide = false;
        if (forwarding() && embed_.cursorHidden()) {
            const NSPoint inWindow = nsWindow_.mouseLocationOutsideOfEventStream;
            hide = windowAt([gameView_ convertPoint:inWindow fromView:nil]) != nullptr;
        }
        showCursor(!hide);
    }

    void showCursor(bool shown) {
        if (shown == !cursorHidden_) {
            return;
        }
        cursorHidden_ = !shown;
        if (shown) {
            [NSCursor unhide];
        } else {
            [NSCursor hide];
        }
    }

    ShellWindow& window_;
    EmbedSocket& embed_;
    NSWindow* nsWindow_;
    Class hostClass_;
    NSView* gameView_{nil};
    id monitor_{nil};
    State state_{State::Idle};
    display::OverlayMode shown_{display::OverlayMode::Hidden};
    Clock::time_point deadline_{};
    NSSize laidOut_{};
    std::vector<Placed> placed_;
    std::map<std::uint64_t, CALayer*> hosts_;
    input::ShortcutChord chord_;
    std::vector<input::Combo> pressed_;
    bool cursorHidden_{false};
};

} // namespace

bool embedsGames() noexcept {
    return false;
}

std::unique_ptr<GameOverlay> openGameOverlay(ShellWindow& window, EmbedSocket& embed) {
    return std::make_unique<LayerHostOverlay>(window, embed);
}

} // namespace opensu::app
