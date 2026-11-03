#include "backlight.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

namespace opensu::host {
namespace {

namespace fs = std::filesystem;

constexpr int minPercent = 1;
constexpr int maxPercent = 100;

int clampPercent(int percent) noexcept {
    return std::clamp(percent, minPercent, maxPercent);
}

std::optional<long> readNumber(const fs::path& file) {
    std::ifstream in{file};
    long value = 0;
    if (!(in >> value) || value < 0) {
        return std::nullopt;
    }
    return value;
}

class LogindBacklight final : public Backlight {
  public:
    LogindBacklight(Runner run, fs::path directory, long max)
        : run_{std::move(run)}, directory_{std::move(directory)}, max_{max} {
    }

    std::optional<int> percent() override {
        const std::optional<long> raw = readNumber(directory_ / "brightness");
        if (!raw) {
            return std::nullopt;
        }
        return static_cast<int>(
            std::lround(static_cast<double>(*raw) * 100.0 / static_cast<double>(max_)));
    }

    std::string setPercent(int percent) override {
        const long raw = std::max(1L, std::lround(static_cast<double>(clampPercent(percent)) *
                                                  static_cast<double>(max_) / 100.0));
        const std::optional<launch::Captured> out =
            run_("busctl", {"--system", "call", "org.freedesktop.login1",
                            "/org/freedesktop/login1/session/auto",
                            "org.freedesktop.login1.Session", "SetBrightness", "ssu", "backlight",
                            directory_.filename().string(), std::to_string(raw)});
        return refusalOf("busctl", out, "set the brightness");
    }

  private:
    Runner run_;
    fs::path directory_;
    long max_;
};

class ReadOnlyBacklight final : public Backlight {
  public:
    explicit ReadOnlyBacklight(std::unique_ptr<Backlight> inner) : inner_{std::move(inner)} {
    }

    std::optional<int> percent() override {
        if (!held_) {
            held_ = inner_->percent();
        }
        return held_;
    }

    std::string setPercent(int percent) override {
        held_ = clampPercent(percent);
        return {};
    }

  private:
    std::unique_ptr<Backlight> inner_;
    std::optional<int> held_;
};

} // namespace

std::unique_ptr<Backlight> makeLogindBacklight(Runner run, const fs::path& root) {
    std::error_code error;
    std::vector<fs::path> devices;
    for (const fs::directory_entry& entry : fs::directory_iterator{root, error}) {
        devices.push_back(entry.path());
    }
    std::ranges::sort(devices);
    for (const fs::path& device : devices) {
        const std::optional<long> max = readNumber(device / "max_brightness");
        if (max && *max > 0 && readNumber(device / "brightness")) {
            return std::make_unique<LogindBacklight>(std::move(run), device, *max);
        }
    }
    return nullptr;
}

std::unique_ptr<Backlight> makeReadOnlyBacklight(std::unique_ptr<Backlight> inner) {
    return std::make_unique<ReadOnlyBacklight>(std::move(inner));
}

} // namespace opensu::host
