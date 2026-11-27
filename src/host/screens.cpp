#include "screens.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <utility>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::host {
namespace {

using json = nlohmann::json;

constexpr const char* kscreenDoctor = "kscreen-doctor";
// libkscreen's Output::Type for a laptop or handheld panel.
constexpr int panelType = 7;

bool builtIn(const json& output, std::string_view connector) {
    return output.value("type", 0) == panelType || connector.starts_with("eDP") ||
           connector.starts_with("LVDS") || connector.starts_with("DSI");
}

std::string nameOf(const json& output, const std::string& connector) {
    if (builtIn(output, connector)) {
        return "Built-in screen";
    }
    constexpr std::pair<std::string_view, std::string_view> kinds[] = {
        {"DP", "DisplayPort"}, {"HDMI", "HDMI"}, {"DVI", "DVI"}, {"VGA", "VGA"}};
    for (const auto& [prefix, kind] : kinds) {
        if (connector.starts_with(prefix)) {
            return std::format("{} ({})", kind, connector);
        }
    }
    return connector;
}

std::string modeOf(const json& output) {
    const std::string current = output.value("currentModeId", std::string{});
    const json modes = output.value("modes", json::array());
    for (const json& mode : modes) {
        if (!mode.is_object() || mode.value("id", std::string{}) != current) {
            continue;
        }
        const json size = mode.value("size", json::object());
        return std::format("{}x{} at {} Hz", size.value("width", 0), size.value("height", 0),
                           std::lround(mode.value("refreshRate", 0.0)));
    }
    return {};
}

class KScreenScreens final : public Screens {
  public:
    KScreenScreens(Runner run, std::string desktopWayland)
        : run_{std::move(run)}, desktopWayland_{std::move(desktopWayland)} {
    }

    std::optional<std::vector<Screen>> read() override {
        if (desktopWayland_.empty()) {
            return std::nullopt;
        }
        const std::optional<launch::Captured> out = doctor({"-j"});
        if (!out || out->status != 0) {
            lucent::warn("screens", "the desktop's display settings: {}",
                         refusalOf(kscreenDoctor, out, "list the screens"));
            return std::nullopt;
        }
        std::optional<std::vector<Screen>> screens = screensOf(out->output);
        if (!screens) {
            lucent::warn("screens", "{} gave no screens: {}", kscreenDoctor,
                         out->output.substr(0, out->output.find('\n')));
        }
        return screens;
    }

    std::string use(const std::string& id) override {
        const std::optional<std::vector<Screen>> screens = read();
        if (!screens) {
            return "the desktop's display settings cannot be read";
        }
        if (std::ranges::find(*screens, id, &Screen::id) == screens->end()) {
            return id + " is not connected";
        }
        std::vector<std::string> args{"output." + id + ".enable", "output." + id + ".priority.1"};
        for (const Screen& other : *screens) {
            if (other.id != id) {
                args.push_back("output." + other.id + ".disable");
            }
        }
        return refusalOf(kscreenDoctor, doctor(args), "switch the screen");
    }

  private:
    /// kscreen-doctor on the desktop's socket, as a Wayland client: openSU's children otherwise
    /// get Gamescope's socket and its Xwayland, where KScreen finds no screens.
    std::optional<launch::Captured> doctor(std::vector<std::string> args) const {
        args.insert(args.begin(), {"WAYLAND_DISPLAY=" + desktopWayland_, "QT_QPA_PLATFORM=wayland",
                                   kscreenDoctor});
        return run_("env", args);
    }

    Runner run_;
    std::string desktopWayland_;
};

class ReadOnlyScreens final : public Screens {
  public:
    ReadOnlyScreens(std::unique_ptr<Screens> inner, std::string reason)
        : inner_{std::move(inner)}, reason_{std::move(reason)} {
    }
    std::optional<std::vector<Screen>> read() override {
        return inner_->read();
    }
    std::string use(const std::string& /*id*/) override {
        return reason_;
    }

  private:
    std::unique_ptr<Screens> inner_;
    std::string reason_;
};

} // namespace

std::optional<std::vector<Screen>> screensOf(std::string_view kscreenJson) {
    const json answer = json::parse(kscreenJson, nullptr, false);
    if (!answer.is_object() || !answer.contains("outputs") || !answer["outputs"].is_array()) {
        return std::nullopt;
    }
    std::vector<Screen> screens;
    std::vector<Screen> others;
    for (const json& output : answer["outputs"]) {
        if (!output.is_object() || !output.value("connected", false)) {
            continue;
        }
        const std::string connector = output.value("name", std::string{});
        if (connector.empty()) {
            continue;
        }
        (builtIn(output, connector) ? screens : others)
            .push_back(Screen{.id = connector,
                              .name = nameOf(output, connector),
                              .mode = modeOf(output),
                              .inUse = output.value("enabled", false) &&
                                       output.value("priority", 0) == 1});
    }
    std::ranges::move(others, std::back_inserter(screens));
    return screens;
}

std::unique_ptr<Screens> makeKScreenScreens(Runner run, std::string desktopWayland) {
    return std::make_unique<KScreenScreens>(std::move(run), std::move(desktopWayland));
}

std::unique_ptr<Screens> makeReadOnlyScreens(std::unique_ptr<Screens> inner, std::string reason) {
    return std::make_unique<ReadOnlyScreens>(std::move(inner), std::move(reason));
}

} // namespace opensu::host
