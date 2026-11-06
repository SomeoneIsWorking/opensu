#include "hosted_runner_window.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "activity_call.hpp"
#include "lucent/log.h"

namespace opensu::retro {
namespace {

using android::ActivityCall;

constexpr std::array<const char*, 2> extensions{VK_KHR_SURFACE_EXTENSION_NAME,
                                                VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};

class HostedRunnerWindow final : public RunnerWindow {
  public:
    HostedRunnerWindow() = default;
    ~HostedRunnerWindow() override {
        release();
    }

    [[nodiscard]] std::span<const char* const> surfaceExtensions() const override {
        return extensions;
    }

    [[nodiscard]] VkSurfaceKHR openSurface(VkInstance instance) override {
        const ActivityCall call;
        if (!call.ready()) {
            throw std::runtime_error{"the runner's context is unavailable"};
        }
        call.env()->CallVoidMethod(call.activity(), call.method("awaitSurface", "()V"));
        // Serial first: a surface that comes between is read again by the next poll.
        serial_ = readSerial(call);
        adopt(call, call.env()->CallObjectMethod(
                        call.activity(), call.method("surface", "()Landroid/view/Surface;")));
        const VkSurfaceKHR surface = makeSurface(instance);
        if (surface == VK_NULL_HANDLE) {
            throw std::runtime_error{"the shell's surface cannot be drawn into"};
        }
        return surface;
    }

    [[nodiscard]] VkSurfaceKHR makeSurface(VkInstance instance) override {
        if (native_ == nullptr) {
            return VK_NULL_HANDLE;
        }
        VkAndroidSurfaceCreateInfoKHR create{};
        create.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        create.window = native_;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (const VkResult made = vkCreateAndroidSurfaceKHR(instance, &create, nullptr, &surface);
            made != VK_SUCCESS) {
            lucent::warn("retro", "vkCreateAndroidSurfaceKHR: {}", static_cast<int>(made));
            return VK_NULL_HANDLE;
        }
        return surface;
    }

    void destroySurface(VkInstance instance, VkSurfaceKHR surface) override {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }

    [[nodiscard]] WindowInput poll() override {
        WindowInput input;
        input.fingers = std::exchange(injected_, {});
        const ActivityCall call;
        if (!call.ready()) {
            return input;
        }
        const jint size =
            call.env()->CallIntMethod(call.activity(), call.method("sizeSerial", "()I"));
        input.resized = std::exchange(sizeSerial_, size) != size;
        if (const jint serial = readSerial(call); serial != serial_) {
            serial_ = serial;
            adopt(call, call.env()->CallObjectMethod(
                            call.activity(), call.method("surface", "()Landroid/view/Surface;")));
            input.surfaceLost = true;
        }
        return input;
    }

    [[nodiscard]] bool hasSurface() const override {
        return native_ != nullptr;
    }

    [[nodiscard]] VkExtent2D pixels() const override {
        if (native_ == nullptr) {
            return {};
        }
        return {static_cast<std::uint32_t>(ANativeWindow_getWidth(native_)),
                static_cast<std::uint32_t>(ANativeWindow_getHeight(native_))};
    }

    // The shell knows the screen: `Presenter::place` says what it reads.
    [[nodiscard]] touch::WindowSurface surface(unsigned width, unsigned height) const override {
        return touch::WindowSurface{.width = width,
                                    .height = height,
                                    .safeWidth = static_cast<float>(width),
                                    .safeHeight = static_cast<float>(height)};
    }

    void inject(const touch::Finger& finger) override {
        injected_.push_back(finger);
    }

  private:
    [[nodiscard]] static jint readSerial(const ActivityCall& call) {
        return call.env()->CallIntMethod(call.activity(), call.method("surfaceSerial", "()I"));
    }

    /// The window of `surface`, null for none, in place of the last.
    void adopt(const ActivityCall& call, jobject surface) {
        release();
        if (const std::string failure = call.takeException(); !failure.empty()) {
            lucent::warn("retro", "the shell's surface cannot be read: {}", failure);
            return;
        }
        native_ = surface != nullptr ? ANativeWindow_fromSurface(call.env(), surface) : nullptr;
        lucent::info("retro", "the shell's surface: {}", native_ != nullptr ? "here" : "gone");
    }

    void release() {
        if (native_ != nullptr) {
            ANativeWindow_release(native_);
            native_ = nullptr;
        }
    }

    ANativeWindow* native_{nullptr};
    jint serial_{0};
    jint sizeSerial_{0};
    std::vector<touch::Finger> injected_;
};

} // namespace

std::unique_ptr<RunnerWindow> openHostedWindow() {
    return std::make_unique<HostedRunnerWindow>();
}

} // namespace opensu::retro
