#include <algorithm>
#include <charconv>
#include <optional>
#include <string_view>

#include "activity_call.hpp"
#include "dns_sd.hpp"
#include "lucent/log.h"
#include "peer_book.hpp"

namespace opensu::lan {
namespace {

using android::ActivityCall;

/// NsdManager through the shell Activity (`LanDiscovery.java`), which holds the multicast lock
/// while it looks.
class NsdDiscovery final : public ServiceDiscovery {
  public:
    explicit NsdDiscovery(std::string name) : name_{std::move(name)} {
    }
    ~NsdDiscovery() override {
        const ActivityCall call;
        if (call.ready()) {
            call.env()->CallVoidMethod(call.activity(), call.method("lanStop", "()V"));
            if (const std::string thrown = call.takeException(); !thrown.empty()) {
                lucent::warn("lan", "NSD did not stop: {}", thrown);
            }
        }
    }
    NsdDiscovery(const NsdDiscovery&) = delete;
    NsdDiscovery& operator=(const NsdDiscovery&) = delete;
    NsdDiscovery(NsdDiscovery&&) = delete;
    NsdDiscovery& operator=(NsdDiscovery&&) = delete;

    [[nodiscard]] std::vector<Sighting> sightings() const override {
        const ActivityCall call;
        if (!call.ready()) {
            return {};
        }
        auto* rows = static_cast<jobjectArray>(call.env()->CallObjectMethod(
            call.activity(), call.method("lanSightings", "()[Ljava/lang/String;")));
        if (!call.takeException().empty() || rows == nullptr) {
            return {};
        }
        std::vector<Sighting> seen;
        for (const std::string& row : call.strings(rows)) {
            std::optional<Sighting> sighting = parse(row);
            if (sighting && !std::ranges::any_of(seen, [&](const Sighting& kept) {
                    return kept.id == sighting->id;
                })) {
                seen.push_back(std::move(*sighting));
            }
        }
        return seen;
    }

    [[nodiscard]] std::string announcedName() const override {
        const ActivityCall call;
        if (!call.ready()) {
            return name_;
        }
        auto* name = static_cast<jstring>(call.env()->CallObjectMethod(
            call.activity(), call.method("lanAnnouncedName", "()Ljava/lang/String;")));
        if (!call.takeException().empty() || name == nullptr) {
            return name_;
        }
        std::string announced = call.string(name);
        return announced.empty() ? name_ : announced;
    }

  private:
    /// "id TAB name TAB address TAB port", as `LanDiscovery.java` keeps a resolved service.
    static std::optional<Sighting> parse(std::string_view row) {
        const std::size_t first = row.find('\t');
        const std::size_t second = row.find('\t', first + 1);
        const std::size_t third = row.find('\t', second + 1);
        if (first == std::string_view::npos || second == std::string_view::npos ||
            third == std::string_view::npos) {
            return std::nullopt;
        }
        Sighting sighting{std::string{row.substr(0, first)},
                          shownName(row.substr(first + 1, second - first - 1)),
                          std::string{row.substr(second + 1, third - second - 1)}, 0};
        const std::string_view port = row.substr(third + 1);
        const auto [end, failed] =
            std::from_chars(port.data(), port.data() + port.size(), sighting.port);
        if (failed != std::errc{} || end != port.data() + port.size() || sighting.port == 0 ||
            !isPeerId(sighting.id)) {
            return std::nullopt;
        }
        return sighting;
    }

    std::string name_;
};

} // namespace

std::string machineName() {
    const ActivityCall call;
    if (!call.ready()) {
        return "openSU";
    }
    auto* name = static_cast<jstring>(call.env()->CallObjectMethod(
        call.activity(), call.method("lanMachineName", "()Ljava/lang/String;")));
    if (!call.takeException().empty() || name == nullptr) {
        return "openSU";
    }
    std::string named = call.string(name);
    return named.empty() ? std::string{"openSU"} : named;
}

std::unique_ptr<ServiceDiscovery> startDiscovery(const Announcement& self, std::string& error) {
    const ActivityCall call;
    if (!call.ready()) {
        error = "the app's Activity cannot be reached";
        return nullptr;
    }
    auto* refusal = static_cast<jstring>(call.env()->CallObjectMethod(
        call.activity(),
        call.method("lanStart", "(Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;"),
        call.env()->NewStringUTF(self.id.c_str()), call.env()->NewStringUTF(self.name.c_str()),
        static_cast<jint>(self.port)));
    if (const std::string thrown = call.takeException(); !thrown.empty()) {
        error = thrown;
        return nullptr;
    }
    if (refusal != nullptr) {
        error = call.string(refusal);
        return nullptr;
    }
    lucent::info("lan", "announcing {} through NSD on port {}", self.name, self.port);
    return std::make_unique<NsdDiscovery>(self.name);
}

} // namespace opensu::lan
