#include "resume_state.hpp"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "lucent/log.h"
#include "resume_file.hpp"
#include "zip/zip_archive.hpp"

namespace opensu::retro {
namespace {

using Clock = std::chrono::system_clock;

std::uint32_t crcOf(std::span<const std::byte> bytes) {
    return zip::crc32Of(
        std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()});
}

std::string labelOf(const ResumeHeader& header) {
    std::string label = header.coreLabel.empty() ? header.core : header.coreLabel;
    if (!header.coreVersion.empty()) {
        label += " " + header.coreVersion;
    }
    return label;
}

} // namespace

ResumeState::ResumeState(std::filesystem::path file, std::string core)
    : file_{std::move(file)}, core_{std::move(core)} {
}

ResumeReport ResumeState::resume(Core& core) {
    std::string error;
    std::optional<ResumeContents> contents = readResume(file_, error);
    if (!contents) {
        if (!error.empty()) {
            lucent::error("retro", "resume state not loaded: {}", error);
            return {.from = std::nullopt, .refused = "It cannot be read"};
        }
        return {};
    }
    const ResumeHeader& header = contents->header;
    if (header.core != core_) {
        lucent::warn("retro", "resume state {} saved by {}, not {}; kept for that core",
                     file_.string(), labelOf(header), core.name());
        return {.from = std::nullopt, .refused = "Saved by " + labelOf(header)};
    }
    std::string refused;
    const std::span<std::byte> ram = core.saveRam();
    if (crcOf(ram) != header.saveRam) {
        refused = "The save changed since";
    } else {
        const std::vector<std::byte> booted{ram.begin(), ram.end()};
        if (!core.unserialize(contents->state)) {
            // A core may have taken part of the state before refusing it.
            core.reset();
            std::ranges::copy(booted, ram.begin());
            refused = core.name() + " refused it";
        }
    }
    if (!refused.empty()) {
        lucent::warn("retro", "resume state {} by {} not loaded on {} {}: {}", file_.string(),
                     labelOf(header), core.name(), core.version(), refused);
    }
    if (!discardResume(file_, error)) {
        lucent::error("retro", "{}", error);
    }
    if (!refused.empty()) {
        return {.from = std::nullopt, .refused = std::move(refused)};
    }
    lucent::info("retro", "resumed from {} ({} bytes of state)", file_.string(),
                 contents->state.size());
    return {.from = header.saved, .refused = {}};
}

StateOutcome ResumeState::suspend(Core& core) {
    const std::vector<std::byte> state = core.serialize();
    if (state.empty()) {
        return {StateResult::Refused, core.name() + " cannot save states"};
    }
    const ResumeHeader header{.core = core_,
                              .coreLabel = core.name(),
                              .coreVersion = core.version(),
                              .saved = Clock::now(),
                              .saveRam = crcOf(core.saveRam())};
    std::string error;
    if (!writeResume(file_, header, state, error)) {
        lucent::error("retro", "resume state not written: {}", error);
        return {StateResult::FileFailed, "the resume state was not written: " + error};
    }
    lucent::info("retro", "saved {} bytes of resume state to {}", state.size(), file_.string());
    return {};
}

} // namespace opensu::retro
