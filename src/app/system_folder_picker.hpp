// system_folder_picker — the OS's own folder picker, where folders are reached only through it:
// Android's document tree picker, whose answer is a `content://` tree URI the app keeps a grant
// for. None on the desktop, where openSU's chooser card lists the filesystem itself.
#pragma once

#include <memory>
#include <optional>
#include <string>

namespace opensu::app {

class SystemFolderPicker {
  public:
    SystemFolderPicker() = default;
    SystemFolderPicker(const SystemFolderPicker&) = delete;
    SystemFolderPicker& operator=(const SystemFolderPicker&) = delete;
    SystemFolderPicker(SystemFolderPicker&&) = delete;
    SystemFolderPicker& operator=(SystemFolderPicker&&) = delete;
    virtual ~SystemFolderPicker() = default;

    /// Shows the picker; whether it could be shown.
    virtual bool open() = 0;
    /// The picker's answer once it closed, taken once: the folder, or "" when cancelled.
    [[nodiscard]] virtual std::optional<std::string> take() = 0;
};

/// This OS's picker; null where openSU lists folders itself.
[[nodiscard]] std::unique_ptr<SystemFolderPicker> openSystemFolderPicker();

} // namespace opensu::app
