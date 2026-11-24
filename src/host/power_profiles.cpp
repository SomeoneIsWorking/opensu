#include "power_profiles.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace opensu::host {
namespace {

using json = nlohmann::json;

constexpr const char* service = "org.shadowblip.PowerStation";
constexpr const char* tdpInterface = "org.shadowblip.GPU.Card.TDP";

/// The TDP interface's properties on `card`, or nothing when it has none.
std::optional<json> propertiesOf(const Runner& run, const std::string& card) {
    const std::optional<launch::Captured> out =
        run("busctl", {"--system", "--json=short", "call", service, card,
                       "org.freedesktop.DBus.Properties", "GetAll", "s", tdpInterface});
    if (!out || out->status != 0) {
        return std::nullopt;
    }
    const json answer = json::parse(out->output, nullptr, false);
    if (!answer.is_object() || !answer.contains("data") || !answer["data"].is_array() ||
        answer["data"].empty() || !answer["data"][0].is_object()) {
        return std::nullopt;
    }
    return answer["data"][0];
}

json valueOf(const json& properties, const char* name) {
    return properties.contains(name) ? properties[name].value("data", json{}) : json{};
}

std::optional<int> wattsOf(const json& properties, const char* name) {
    const json value = valueOf(properties, name);
    return value.is_number()
               ? std::optional<int>{static_cast<int>(std::lround(value.get<double>()))}
               : std::nullopt;
}

class PowerStationProfiles final : public PowerProfiles {
  public:
    PowerStationProfiles(Runner run, std::string card)
        : run_{std::move(run)}, card_{std::move(card)} {
    }

    std::optional<PowerProfileState> read() override {
        const std::optional<json> properties = propertiesOf(run_, card_);
        if (!properties) {
            return std::nullopt;
        }
        PowerProfileState state;
        const json profile = valueOf(*properties, "PowerProfile");
        const json available = valueOf(*properties, "PowerProfilesAvailable");
        if (!profile.is_string() || !available.is_array()) {
            return std::nullopt;
        }
        state.profile = profile.get<std::string>();
        for (const json& each : available) {
            if (each.is_string()) {
                state.profiles.push_back(each.get<std::string>());
            }
        }
        return state;
    }

    std::string set(const std::string& profile) override {
        const std::optional<json> properties = propertiesOf(run_, card_);
        const std::optional<int> low =
            properties ? wattsOf(*properties, "MinTdp") : std::optional<int>{};
        const std::optional<int> high =
            properties ? wattsOf(*properties, "MaxTdp") : std::optional<int>{};
        if (!low || !high || *low > *high) {
            return "PowerStation did not give the power limit's range";
        }
        std::string refused = setProperty("PowerProfile", "s", profile, "set the power profile");
        if (!refused.empty()) {
            return refused;
        }
        return setProperty("TDP", "d", std::to_string(profileWatts(profile, *low, *high)),
                           "set the power limit");
    }

  private:
    std::string setProperty(const char* property, const char* signature, const std::string& value,
                            const std::string& what) {
        return refusalOf("busctl",
                         run_("busctl", {"--system", "set-property", service, card_, tdpInterface,
                                         property, signature, value}),
                         what);
    }

    Runner run_;
    std::string card_;
};

class ReadOnlyPowerProfiles final : public PowerProfiles {
  public:
    ReadOnlyPowerProfiles(std::unique_ptr<PowerProfiles> inner, std::string reason)
        : inner_{std::move(inner)}, reason_{std::move(reason)} {
    }
    std::optional<PowerProfileState> read() override {
        return inner_->read();
    }
    std::string set(const std::string& /*profile*/) override {
        return reason_;
    }

  private:
    std::unique_ptr<PowerProfiles> inner_;
    std::string reason_;
};

} // namespace

int profileWatts(const std::string& profile, int minWatts, int maxWatts) {
    const int watts = profile == "quiet"      ? maxWatts / 3
                      : profile == "balanced" ? maxWatts / 2
                                              : maxWatts;
    return std::clamp(watts, minWatts, maxWatts);
}

std::unique_ptr<PowerProfiles> makePowerStationProfiles(Runner run) {
    const std::optional<launch::Captured> out =
        run("busctl", {"--system", "--list", "tree", service});
    if (!out || out->status != 0) {
        return nullptr;
    }
    const std::regex card{"^/org/shadowblip/Performance/GPU/card[0-9]+$"};
    std::istringstream lines{out->output};
    for (std::string line; std::getline(lines, line);) {
        if (std::regex_match(line, card) && propertiesOf(run, line)) {
            auto profiles = std::make_unique<PowerStationProfiles>(std::move(run), line);
            return profiles->read() ? std::move(profiles) : nullptr;
        }
    }
    return nullptr;
}

std::unique_ptr<PowerProfiles> makeReadOnlyPowerProfiles(std::unique_ptr<PowerProfiles> inner,
                                                         std::string reason) {
    return inner ? std::make_unique<ReadOnlyPowerProfiles>(std::move(inner), std::move(reason))
                 : nullptr;
}

} // namespace opensu::host
