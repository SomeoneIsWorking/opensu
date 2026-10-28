#include "night_light.hpp"

#include "host/desktop_night_light.hpp"
#include "host/runner.hpp"

namespace opensu::app {
namespace {

/// KDE Plasma's night light: KWin's settings and knighttimed's schedule.
class KdeDesktopNight final : public DesktopNight {
  public:
    explicit KdeDesktopNight(const std::filesystem::path& xdgConfigHome)
        : desktop_{host::systemRunner(), xdgConfigHome / "kwinrc"} {
    }

    int kelvinAt(std::chrono::system_clock::time_point now) override {
        return desktop_.kelvinAt(now);
    }
    [[nodiscard]] int nightKelvin() const override {
        return desktop_.settings().nightKelvin;
    }

  private:
    host::DesktopNightLight desktop_;
};

} // namespace

std::unique_ptr<DesktopNight> openDesktopNight(const std::filesystem::path& xdgConfigHome) {
    return std::make_unique<KdeDesktopNight>(xdgConfigHome);
}

} // namespace opensu::app
