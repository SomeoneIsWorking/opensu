#include "input_plumber.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::host {
namespace {

using json = nlohmann::json;

constexpr const char* service = "org.shadowblip.InputPlumber";
constexpr const char* compositeInterface = "org.shadowblip.Input.CompositeDevice";
constexpr const char* targetInterface = "org.shadowblip.Input.Target";
constexpr std::string_view compositePrefix = "/org/shadowblip/InputPlumber/CompositeDevice";
/// Not the Xbox 360 pad openSU's virtual pads are, which a game must tell apart by its ids.
constexpr std::string_view xboxSeries = "xbox-series";

/// Target ids whose pad is a Steam Deck HID device: Steam reads it, nothing else does.
constexpr std::array<std::string_view, 2> steamOnly{"deck", "deck-uhid"};

/// The `data` of a `busctl --json=short get-property` answer, or null.
json propertyOf(const Runner& run, const std::string& path, const char* interface,
                const char* name) {
    const std::optional<launch::Captured> out =
        run("busctl", {"--system", "--json=short", "get-property", service, path, interface, name});
    if (!out || out->status != 0) {
        return nullptr;
    }
    const json answer = json::parse(out->output, nullptr, false);
    return answer.is_object() ? answer.value("data", json{}) : json{};
}

/// InputPlumber's composite devices; none when it is not running.
std::vector<std::string> compositeDevices(const Runner& run) {
    const std::optional<launch::Captured> out =
        run("busctl", {"--system", "--list", "tree", service});
    std::vector<std::string> devices;
    if (!out || out->status != 0) {
        return devices;
    }
    std::istringstream lines{out->output};
    for (std::string line; std::getline(lines, line);) {
        const std::string_view path{line};
        if (path.starts_with(compositePrefix) &&
            path.find('/', compositePrefix.size()) == std::string_view::npos) {
            devices.push_back(line);
        }
    }
    return devices;
}

/// The target ids of `device`'s targets, in order; nothing when one cannot be read.
std::optional<std::vector<std::string>> targetsOf(const Runner& run, const std::string& device) {
    const json paths = propertyOf(run, device, compositeInterface, "TargetDevices");
    if (!paths.is_array()) {
        return std::nullopt;
    }
    std::vector<std::string> targets;
    for (const json& path : paths) {
        const json type = path.is_string() ? propertyOf(run, path.get<std::string>(),
                                                        targetInterface, "DeviceType")
                                           : json{};
        if (!type.is_string()) {
            return std::nullopt;
        }
        targets.push_back(type.get<std::string>());
    }
    return targets;
}

std::string joined(const std::vector<std::string>& targets) {
    std::string text;
    for (const std::string& target : targets) {
        text += (text.empty() ? "" : ", ") + target;
    }
    return text;
}

} // namespace

std::optional<std::vector<std::string>> readableTargets(const std::vector<std::string>& targets) {
    std::vector<std::string> readable;
    bool replaced = false;
    for (const std::string& target : targets) {
        const bool onlySteam = std::ranges::find(steamOnly, target) != steamOnly.end();
        replaced = replaced || onlySteam;
        const std::string next = onlySteam ? std::string{xboxSeries} : target;
        if (std::ranges::find(readable, next) == readable.end()) {
            readable.push_back(next);
        }
    }
    if (!replaced) {
        return std::nullopt;
    }
    return readable;
}

namespace {

class InputPlumberPads final : public ReadablePads {
  public:
    explicit InputPlumberPads(Runner run) : run_{std::move(run)} {
        for (const std::string& device : compositeDevices(run_)) {
            const std::optional<std::vector<std::string>> targets = targetsOf(run_, device);
            const std::optional<std::vector<std::string>> readable =
                targets ? readableTargets(*targets) : std::nullopt;
            if (readable && setTargets(device, *readable)) {
                lucent::info("gamepad",
                             "InputPlumber's {} presents {} in place of {} while openSU runs",
                             device, joined(*readable), joined(*targets));
                changed_.emplace_back(device, *targets);
            }
        }
    }
    ~InputPlumberPads() override {
        for (const auto& [device, targets] : changed_) {
            if (setTargets(device, targets)) {
                lucent::info("gamepad", "InputPlumber's {} presents {} again", device,
                             joined(targets));
            }
        }
    }
    InputPlumberPads(const InputPlumberPads&) = delete;
    InputPlumberPads& operator=(const InputPlumberPads&) = delete;
    InputPlumberPads(InputPlumberPads&&) = delete;
    InputPlumberPads& operator=(InputPlumberPads&&) = delete;

  private:
    /// Sets `device`'s targets to `targets`; whether InputPlumber took them.
    bool setTargets(const std::string& device, const std::vector<std::string>& targets) {
        std::vector<std::string> args{"--system",
                                      "call",
                                      service,
                                      device,
                                      compositeInterface,
                                      "SetTargetDevices",
                                      "as",
                                      std::to_string(targets.size())};
        args.insert(args.end(), targets.begin(), targets.end());
        const std::optional<launch::Captured> out = run_("busctl", args);
        const std::string refusal = refusalOf("busctl", out, "set InputPlumber's targets");
        if (!refusal.empty()) {
            lucent::warn("gamepad", "InputPlumber's {} kept its targets: {}", device, refusal);
        }
        return refusal.empty();
    }

    Runner run_;
    /// Each composite device changed, with the targets it had.
    std::vector<std::pair<std::string, std::vector<std::string>>> changed_;
};

} // namespace

std::unique_ptr<ReadablePads> makeReadablePads(Runner run) {
    return std::make_unique<InputPlumberPads>(std::move(run));
}

} // namespace opensu::host
