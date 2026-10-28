#include "activity_call.hpp"

#include <SDL3/SDL_system.h>

namespace opensu::android {

ActivityCall::ActivityCall()
    : env_{static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv())},
      activity_{static_cast<jobject>(SDL_GetAndroidActivity())} {
    if (env_ != nullptr && activity_ != nullptr && env_->PushLocalFrame(32) == 0) {
        framed_ = true;
        class_ = env_->GetObjectClass(activity_);
    }
}

ActivityCall::~ActivityCall() {
    if (framed_) {
        env_->PopLocalFrame(nullptr);
    }
    // SDL_GetAndroidActivity hands out a local reference made outside the frame.
    if (env_ != nullptr && activity_ != nullptr) {
        env_->DeleteLocalRef(activity_);
    }
}

jmethodID ActivityCall::method(const char* name, const char* signature) const {
    return env_->GetMethodID(class_, name, signature);
}

std::string ActivityCall::takeException() const {
    if (env_->ExceptionCheck() == JNI_FALSE) {
        return {};
    }
    jthrowable thrown = env_->ExceptionOccurred();
    env_->ExceptionClear();
    jmethodID text =
        env_->GetMethodID(env_->GetObjectClass(thrown), "toString", "()Ljava/lang/String;");
    auto* message = static_cast<jstring>(env_->CallObjectMethod(thrown, text));
    env_->ExceptionClear();
    return message != nullptr ? string(message) : std::string{"a Java exception"};
}

std::string ActivityCall::string(jstring text) const {
    const char* chars = env_->GetStringUTFChars(text, nullptr);
    std::string out{chars != nullptr ? chars : ""};
    if (chars != nullptr) {
        env_->ReleaseStringUTFChars(text, chars);
    }
    return out;
}

std::vector<std::string> ActivityCall::strings(jobjectArray values) const {
    std::vector<std::string> out;
    const jsize count = env_->GetArrayLength(values);
    out.reserve(static_cast<std::size_t>(count));
    for (jsize i = 0; i < count; ++i) {
        auto* value = static_cast<jstring>(env_->GetObjectArrayElement(values, i));
        out.push_back(value != nullptr ? string(value) : std::string{});
    }
    return out;
}

jobjectArray ActivityCall::strings(const std::vector<std::string>& values) const {
    jobjectArray array = env_->NewObjectArray(static_cast<jsize>(values.size()),
                                              env_->FindClass("java/lang/String"), nullptr);
    for (std::size_t i = 0; i < values.size(); ++i) {
        env_->SetObjectArrayElement(array, static_cast<jsize>(i),
                                    env_->NewStringUTF(values[i].c_str()));
    }
    return array;
}

} // namespace opensu::android
