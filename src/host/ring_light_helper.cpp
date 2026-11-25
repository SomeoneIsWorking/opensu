#include "ring_light_helper.hpp"

#include <array>
#include <cstdio>
#include <utility>

#include "session_scripts.hpp"

namespace opensu::host {
namespace {

constexpr const char* installerName = "install-ring-lights.sh";
constexpr const char* stagedHelperName = "opensu-ring-lights";

class SudoRingLightHelper final : public RingLightHelper {
  public:
    SudoRingLightHelper(Runner run, RingLightHelperPaths paths)
        : run_{std::move(run)}, paths_{std::move(paths)} {
    }

    bool installed() const override {
        std::error_code error;
        return std::filesystem::is_regular_file(paths_.helper, error);
    }
    std::string installBlocker() const override {
        return rootStepBlocker(ringLightInstallPlan(paths_));
    }
    RootStep stage() override {
        return stageRootStep(ringLightInstallPlan(paths_));
    }
    InstallResult install() override {
        return runRootStep(run_, ringLightInstallPlan(paths_), [this] {
            return installed();
        });
    }
    std::string set(std::uint32_t rgb) override {
        std::array<char, 7> hex{};
        std::snprintf(hex.data(), hex.size(), "%06x", rgb & 0xFFFFFFU);
        return refusalOf("sudo", run_("sudo", {"-n", paths_.helper.string(), hex.data()}),
                         "set the stick lights");
    }

  private:
    Runner run_;
    RingLightHelperPaths paths_;
};

class ReadOnlyRingLightHelper final : public RingLightHelper {
  public:
    ReadOnlyRingLightHelper(std::unique_ptr<RingLightHelper> inner, std::string reason,
                            bool standInStep)
        : inner_{std::move(inner)}, reason_{std::move(reason)}, standInStep_{standInStep} {
    }

    bool installed() const override {
        return inner_->installed();
    }
    std::string installBlocker() const override {
        return inner_->installBlocker();
    }
    RootStep stage() override {
        return inner_->stage();
    }
    InstallResult install() override {
        if (standInStep_) {
            return inner_->install();
        }
        return {InstallResult::Kind::Failed, reason_};
    }
    std::string set(std::uint32_t /*rgb*/) override {
        return reason_;
    }

  private:
    std::unique_ptr<RingLightHelper> inner_;
    std::string reason_;
    bool standInStep_;
};

} // namespace

RootStepPlan ringLightInstallPlan(const RingLightHelperPaths& paths) {
    return RootStepPlan{.tag = "lights",
                        .what = "the stick lights' helper",
                        .stage = paths.stage,
                        .pkexec = paths.pkexec,
                        .files = {{installerName, std::string{installRingLightsScript}},
                                  {stagedHelperName, std::string{ringLightsScript}}},
                        .installer = installerName,
                        .standIn = paths.rootStepStandIn};
}

std::unique_ptr<RingLightHelper> makeSudoRingLightHelper(Runner run, RingLightHelperPaths paths) {
    return std::make_unique<SudoRingLightHelper>(std::move(run), std::move(paths));
}

std::unique_ptr<RingLightHelper> makeReadOnlyRingLightHelper(std::unique_ptr<RingLightHelper> inner,
                                                             std::string reason,
                                                             bool standInStep) {
    return std::make_unique<ReadOnlyRingLightHelper>(std::move(inner), std::move(reason),
                                                     standInStep);
}

} // namespace opensu::host
