#include "shell.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "lucent/log.h"

namespace opensu::ui {
namespace {

// STOPGAP: Android reports a density and a desktop does not, so a dp is the window mapped onto
// the 853 x 480 dp canvas iiSU's renderer scales against (tb0.g).
constexpr float referenceWidthDp = 853.0f;
constexpr float referenceHeightDp = 480.0f;

} // namespace

Shell::Shell(int width, int height, config::HomeMode mode, const std::filesystem::path& assetsDir)
    : text_{assetsDir}, icons_{assetsDir, images_}, grid_{mode, text_, icons_, images_},
      width_{width}, height_{height} {
    hud_.setSize({width, height, dp()});
    frameGrid();
    // The pointer reaches the last added first: the keyboard, the mapping editor, the dock, the
    // top bar.
    gpu2d::ui::Navigator& navigator = screens_.navigator();
    navigator.addChrome(topBar_);
    navigator.addChrome(dock_);
    navigator.addChrome(mappingChrome_);
    navigator.addChrome(keyboardChrome_);
}

void Shell::attach(SDL_GPUDevice& device) {
    text_.load(device);
    device_ = &device;
}

SDL_GPUDevice& Shell::device() const {
    if (device_ == nullptr) {
        throw std::logic_error{"the shell drew before it was attached to a device"};
    }
    return *device_;
}

float Shell::dp() const noexcept {
    return std::min(static_cast<float>(width_) / referenceWidthDp,
                    static_cast<float>(height_) / referenceHeightDp) *
           uiScale_;
}

void Shell::frameGrid() {
    grid_.setFrame(HomeGrid::Frame{.width = width_,
                                   .height = height_,
                                   .dp = dp(),
                                   .topInset = hud_.topInset(),
                                   .bottomInset = hud_.bottomInset()});
}

void Shell::setSize(int width, int height) {
    width_ = width;
    height_ = height;
    hud_.setSize({width, height, dp()});
    frameGrid();
}

void Shell::setUiScale(float factor) {
    if (factor != uiScale_) {
        uiScale_ = factor;
        hud_.setSize({width_, height_, dp()});
        frameGrid();
    }
}

void Shell::loadArtwork(std::size_t bytes) {
    grid_.requestArtwork();
    land(images_.upload(device(), bytes));
}

void Shell::loadArtworkNow() {
    grid_.requestArtwork();
    images_.waitAll();
    land(images_.upload(device(), std::numeric_limits<std::size_t>::max()));
}

bool Shell::settleIcons() {
    if (!icons_.pending()) {
        return false;
    }
    icons_.wait();
    land(images_.upload(device(), std::numeric_limits<std::size_t>::max()));
    return true;
}

void Shell::land(const gpu2d::ImageUploads& uploads) {
    for (const gpu2d::ImageFailure& failure : uploads.failures) {
        lucent::warn("ui", "image {} unreadable: {}", failure.key, failure.error);
    }
}

void Shell::setSection(library::Section section, bool revealDock) {
    grid_.setSection(section);
    dock_.setSection(section, revealDock);
}

void Shell::tick(Clock::time_point now) {
    now_ = now;
    grid_.tick(now);
    volumeOsd_.tick(now_);
    hud_.tick(now);
    const gpu2d::ui::Navigator& navigator = screens_.navigator();
    const auto width = static_cast<float>(width_);
    const auto height = static_cast<float>(height_);
    const ChromeFrame frame{width, height, dp(), now_};
    keyboardChrome_.setFrame(frame);
    mappingChrome_.setFrame(frame);
    const bool pages = navigator.pagesVisible();
    grid_.showPageControls(!pages);
    dock_.setFrame(HomeDock::Frame{.width = width,
                                   .height = height,
                                   .dp = dp(),
                                   .now = now_,
                                   .shown = !inGame_ && !pages && !keyboard_.isOpen(),
                                   .live = !inGame_ && navigator.atRoot() && !keyboard_.isOpen() &&
                                           !mapping_.isOpen()});
    // Overlays stand above the docked keyboard, so the field it types into shows.
    screens_.layout(now_, gpu2d::Rect{0.0f, 0.0f, width, height}, height - keyboardChrome_.top(),
                    dp(), Chrome{hud_.topInset()});
}

void Shell::settle() {
    grid_.settle();
}

gpu2d::RenderTarget& Shell::sceneTarget() {
    if (!scene_ || scene_->width() != width_ || scene_->height() != height_) {
        scene_.emplace(device(), width_, height_);
    }
    return *scene_;
}

void Shell::drawChordHint(Canvas& canvas, Point size) {
    if (chordHint_.visible(now_)) {
        chordHintPainter_.paint(canvas, chordHint_, size, dp(), chordHint_.look(now_));
    }
}

void Shell::draw(Canvas& canvas, const gpu2d::Surface& target) {
    // The previous frame was submitted, so the labels it did not draw can go.
    text_.retire();
    const Point size{static_cast<float>(target.width), static_cast<float>(target.height)};
    gpu2d::ui::Navigator& navigator = screens_.navigator();
    // Transparent, so a compositor shows what lies below wherever nothing is drawn.
    constexpr Colour clear{0, 0, 0, 0};
    if (inGame_) {
        // Only the Guide menu and what it opens are drawn; the game shows through everywhere else.
        canvas.begin(target, clear);
        screens_.paintPages(canvas);
        // A panel over the game (a game's settings) leaves the top bar out; a page has it.
        if (navigator.pagesVisible()) {
            navigator.paintChrome(canvas, topBar_);
            hud_.drawHints(canvas);
        }
        navigator.paintChrome(canvas, mappingChrome_);
        screens_.paintOverlays(canvas);
        navigator.paintChrome(canvas, keyboardChrome_);
        drawChordHint(canvas, size);
        hud_.drawToast(canvas, keyboardChrome_.top());
        return;
    }

    // The scene goes to its own target so the dock's glass can blur what is behind it.
    const gpu2d::RenderTarget& scene = sceneTarget();
    canvas.begin(scene.surface(), clear);
    hud_.drawGround(canvas);
    screens_.paintPages(canvas);
    navigator.paintChrome(canvas, topBar_);
    hud_.drawHints(canvas);

    canvas.begin(target, clear);
    const gpu2d::Rect whole{0.0f, 0.0f, static_cast<float>(scene.width()),
                            static_cast<float>(scene.height())};
    canvas.image(scene.view(gpu2d::Filter::Nearest), whole, whole, white);
    dock_.setScene(&scene);
    navigator.paintChrome(canvas, dock_);
    const float toastFloor = std::min(keyboardChrome_.top(), dock_.top().value_or(size.y));
    navigator.paintChrome(canvas, mappingChrome_);
    screens_.paintOverlays(canvas);
    navigator.paintChrome(canvas, keyboardChrome_);
    if (volumeOsd_.visible(now_)) {
        volumeOsdPainter_.paint(canvas, volumeOsd_.level(), size, dp(), hud_.topInset(),
                                volumeOsd_.look(now_));
    }
    drawChordHint(canvas, size);
    hud_.drawToast(canvas, toastFloor);
}

} // namespace opensu::ui
