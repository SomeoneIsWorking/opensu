// input_plumber — the pads InputPlumber presents (a handheld's built-in controller on Bazzite,
// ChimeraOS and the like). Its Steam Deck targets are HID devices only Steam reads; while
// `ReadablePads` lives each one is an Xbox 360 pad instead, an input device openSU and its games
// read, and the targets it found are put back when it goes.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "runner.hpp"

namespace opensu::host {

/// `targets` (InputPlumber's target device ids for one composite device) with each Steam-only
/// gamepad an Xbox 360 pad; nothing when none is Steam-only.
[[nodiscard]] std::optional<std::vector<std::string>>
readableTargets(const std::vector<std::string>& targets);

class ReadablePads {
  public:
    ReadablePads() = default;
    virtual ~ReadablePads() = default;
    ReadablePads(const ReadablePads&) = delete;
    ReadablePads& operator=(const ReadablePads&) = delete;
    ReadablePads(ReadablePads&&) = delete;
    ReadablePads& operator=(ReadablePads&&) = delete;
};

/// InputPlumber's Steam-only pads made readable over `run`, until the result goes.
[[nodiscard]] std::unique_ptr<ReadablePads> makeReadablePads(Runner run);

} // namespace opensu::host
