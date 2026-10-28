// activity_call — one call from native code into the shell's Activity: this thread's JNIEnv, the
// Activity, and a local reference frame that frees every reference the call made.
#pragma once

#include <jni.h>

#include <string>
#include <vector>

namespace opensu::android {

class ActivityCall {
  public:
    ActivityCall();
    ActivityCall(const ActivityCall&) = delete;
    ActivityCall& operator=(const ActivityCall&) = delete;
    ActivityCall(ActivityCall&&) = delete;
    ActivityCall& operator=(ActivityCall&&) = delete;
    ~ActivityCall();

    /// Whether the Activity can be called; false before SDL has one or when JNI is out of memory.
    [[nodiscard]] bool ready() const noexcept {
        return framed_ && class_ != nullptr;
    }
    [[nodiscard]] JNIEnv* env() const noexcept {
        return env_;
    }
    [[nodiscard]] jobject activity() const noexcept {
        return activity_;
    }
    /// The Activity's method `name` with JNI `signature`.
    [[nodiscard]] jmethodID method(const char* name, const char* signature) const;

    /// The pending Java exception's message, cleared; empty when none is pending.
    [[nodiscard]] std::string takeException() const;
    [[nodiscard]] std::string string(jstring text) const;
    [[nodiscard]] jobjectArray strings(const std::vector<std::string>& values) const;
    /// A Java String[]'s elements; a null element reads as empty.
    [[nodiscard]] std::vector<std::string> strings(jobjectArray values) const;

  private:
    JNIEnv* env_;
    jobject activity_;
    jclass class_{nullptr};
    bool framed_{false};
};

} // namespace opensu::android
