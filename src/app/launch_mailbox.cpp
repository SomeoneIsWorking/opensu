#include "launch_mailbox.hpp"

#include <utility>

namespace opensu::app {

void LaunchMailbox::postProgress(const launch::LaunchProgress& progress) {
    const std::lock_guard lock{mutex_};
    progress_ = progress;
}

void LaunchMailbox::postToast(std::string text, bool error) {
    const std::lock_guard lock{mutex_};
    toast_ = Toast{std::move(text), error};
}

std::optional<launch::LaunchProgress> LaunchMailbox::takeProgress() {
    const std::lock_guard lock{mutex_};
    return std::exchange(progress_, std::nullopt);
}

std::optional<LaunchMailbox::Toast> LaunchMailbox::takeToast() {
    const std::lock_guard lock{mutex_};
    return std::exchange(toast_, std::nullopt);
}

} // namespace opensu::app
