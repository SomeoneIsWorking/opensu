#include "activity_call.hpp"
#include "system_folder_picker.hpp"

namespace opensu::app {

namespace {

using android::ActivityCall;

/// ACTION_OPEN_DOCUMENT_TREE through the shell Activity, which keeps the grant.
class DocumentTreePicker final : public SystemFolderPicker {
  public:
    bool open() override {
        const ActivityCall call;
        if (!call.ready()) {
            return false;
        }
        call.env()->CallVoidMethod(call.activity(), call.method("pickFolder", "()V"));
        return call.takeException().empty();
    }

    [[nodiscard]] std::optional<std::string> take() override {
        const ActivityCall call;
        if (!call.ready()) {
            return std::nullopt;
        }
        auto* answer = static_cast<jstring>(call.env()->CallObjectMethod(
            call.activity(), call.method("takePickedFolder", "()Ljava/lang/String;")));
        if (!call.takeException().empty() || answer == nullptr) {
            return std::nullopt;
        }
        return call.string(answer);
    }
};

} // namespace

std::unique_ptr<SystemFolderPicker> openSystemFolderPicker() {
    return std::make_unique<DocumentTreePicker>();
}

} // namespace opensu::app
