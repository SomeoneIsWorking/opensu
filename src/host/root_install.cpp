#include "root_install.hpp"

#include <cctype>
#include <optional>
#include <stdexcept>

#include "fileio/owner_only_write.hpp"
#include "lucent/log.h"

namespace opensu::host {
namespace {

std::string lastLine(const std::string& output) {
    const std::size_t end = output.find_last_not_of('\n');
    if (end == std::string::npos) {
        return {};
    }
    const std::size_t newline = output.find_last_of('\n', end);
    const std::size_t start = newline == std::string::npos ? 0 : newline + 1;
    return output.substr(start, end - start + 1);
}

std::string capitalised(std::string text) {
    if (!text.empty()) {
        text.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(text.front())));
    }
    return text;
}

} // namespace

std::string RootStep::shown() const {
    std::string line = program;
    for (const std::string& arg : args) {
        line += ' ';
        line += arg;
    }
    return line;
}

std::string rootStepBlocker(const RootStepPlan& plan) {
    if (plan.pkexec.empty()) {
        return "Installing " + plan.what +
               " needs polkit's pkexec: sudo dnf install polkit (Fedora), sudo apt install pkexec "
               "(Debian, Ubuntu), sudo pacman -S polkit (Arch)";
    }
    return {};
}

RootStep stageRootStep(const RootStepPlan& plan) {
    for (const StagedFile& file : plan.files) {
        const std::filesystem::path path = plan.stage / file.name;
        std::string error;
        if (!fileio::writeOwnerOnly(path, file.bytes, error)) {
            throw std::runtime_error{"could not write " + path.string() + ": " + error};
        }
    }
    if (!plan.standIn.empty()) {
        return RootStep{.program = plan.pkexec.string(), .args = {plan.standIn.string()}};
    }
    return RootStep{.program = plan.pkexec.string(),
                    .args = {"/bin/sh", (plan.stage / plan.installer).string()}};
}

InstallResult runRootStep(const Runner& run, const RootStepPlan& plan,
                          const std::function<bool()>& inPlace) {
    if (const std::string blocked = rootStepBlocker(plan); !blocked.empty()) {
        return {InstallResult::Kind::Failed, blocked};
    }
    RootStep step;
    try {
        step = stageRootStep(plan);
    } catch (const std::runtime_error& error) {
        lucent::warn(plan.tag, "staging {} failed: {}", plan.what, error.what());
        return {InstallResult::Kind::Failed,
                "The files for " + plan.what + " could not be put together: " + error.what()};
    }
    lucent::info(plan.tag, "installing {}: {}", plan.what, step.shown());
    const std::optional<launch::Captured> out = run(step.program, step.args);
    if (!out) {
        return {InstallResult::Kind::Failed, "pkexec could not be run"};
    }
    if (out->status == 0) {
        if (!inPlace()) {
            lucent::warn(plan.tag, "the installer finished, but {} is not in place", plan.what);
            return {InstallResult::Kind::Failed,
                    "The installer finished, but " + plan.what + " is not in place"};
        }
        lucent::info(plan.tag, "{} installed", plan.what);
        return {};
    }
    const bool began = out->output.find(installerMarker) != std::string::npos;
    if (!began && out->status == pkexecDismissed) {
        lucent::info(plan.tag, "installing {} was cancelled at authentication", plan.what);
        return {InstallResult::Kind::Cancelled, capitalised(plan.what) + " was not installed"};
    }
    const std::string reason = lastLine(out->output);
    lucent::warn(plan.tag, "installing {} failed ({}): {}", plan.what, out->status, reason);
    return {InstallResult::Kind::Failed,
            reason.empty() ? "pkexec refused to install " + plan.what : reason};
}

} // namespace opensu::host
