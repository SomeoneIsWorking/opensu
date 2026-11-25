// ring_light_helper — the stick lights' colour where the kernel's LED class cannot carry it (the
// ROG Ally's driver caps each packed zone at 255). A root-owned helper sends the controller's own
// RGB command (`packaging/opensu-ring-lights.sh`); one root step installs it with a sudoers rule
// that lets the user run that helper and nothing else, and openSU runs it through `sudo -n`.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "root_install.hpp"
#include "runner.hpp"

namespace opensu::host {

/// Where the installer puts the helper, and what its sudoers rule is called.
inline constexpr const char* ringLightHelperPath = "/usr/local/libexec/opensu-ring-lights";
inline constexpr const char* ringLightRulePath = "/etc/sudoers.d/zz-opensu-ring-lights";

struct RingLightHelperPaths {
    std::filesystem::path helper{ringLightHelperPath};
    /// Where the root step's files are put together: `<runtime dir>/ring-lights-install`.
    std::filesystem::path stage;
    /// polkit's pkexec, as found on PATH; empty when there is none.
    std::filesystem::path pkexec;
    /// What pkexec runs in place of the installer, for a test run; empty for the installer.
    std::filesystem::path rootStepStandIn;
};

/// The root step that installs the helper and its sudoers rule, staged in `paths.stage`.
[[nodiscard]] RootStepPlan ringLightInstallPlan(const RingLightHelperPaths& paths);

/// Installed when the helper in place is the one this openSU carries; an older one still runs
/// `set` but not `rainbow`, and installing again replaces it.
class RingLightHelper : public RootInstall {
  public:
    /// Whether a helper of any version is in place.
    [[nodiscard]] virtual bool runs() const = 0;
    /// Sets every ring to `rgb` (0xRRGGBB; 0 turns them off). Empty when done, else why not.
    virtual std::string set(std::uint32_t rgb) = 0;
    /// The controller's own rainbow at `level`, 1 to 3. Empty when done, else why not.
    virtual std::string rainbow(int level) = 0;
};

/// The helper over `run`: pkexec to install it, `sudo -n` to run it.
[[nodiscard]] std::unique_ptr<RingLightHelper> makeSudoRingLightHelper(Runner run,
                                                                      RingLightHelperPaths paths);

/// `inner` for reading whether it is installed; installing and setting refuse with `reason`, unless
/// the root step is a stand-in, which installs nothing and runs. For hidden runs.
[[nodiscard]] std::unique_ptr<RingLightHelper>
makeReadOnlyRingLightHelper(std::unique_ptr<RingLightHelper> inner, std::string reason,
                            bool standInStep);

} // namespace opensu::host
