#include "night_light.hpp"

#include <utility>

#include "lucent/log.h"

namespace opensu::app {
namespace {

constexpr int neutralKelvin = 6500;
/// KWin moves its temperature in 50 K steps over a transition of half an hour or so.
constexpr std::chrono::seconds lookEvery{10};

} // namespace

NightLight::NightLight(std::unique_ptr<DesktopNight> desktop,
                       std::unique_ptr<NightLightOutput> output)
    : desktop_{std::move(desktop)}, output_{std::move(output)} {
}

int NightLight::kelvinFor(settings::NightLightChoice choice,
                          std::chrono::system_clock::time_point now) {
    if (!desktop_) {
        return neutralKelvin;
    }
    switch (choice) {
    case settings::NightLightChoice::Desktop:
        return desktop_->kelvinAt(now);
    case settings::NightLightChoice::Off:
        return neutralKelvin;
    case settings::NightLightChoice::On:
        return desktop_->nightKelvin();
    }
    return neutralKelvin;
}

void NightLight::service(settings::NightLightChoice choice,
                         std::chrono::system_clock::time_point now) {
    if (choice_ == choice && looked_ && now - *looked_ < lookEvery) {
        return;
    }
    choice_ = choice;
    looked_ = now;
    kelvin_ = kelvinFor(choice, now);
    if (!output_ || shown_ == kelvin_) {
        return;
    }
    output_->show(kelvin_);
    shown_ = kelvin_;
    lucent::info("nightlight", "{}: {} K", settings::label(choice), kelvin_);
}

} // namespace opensu::app
