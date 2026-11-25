#include "pad_calibration.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opensu::gamepad {
namespace {

/// Axes 2 and 5 (`ABS_Z`, `ABS_RZ`) are the triggers; the rest are sticks.
constexpr std::array<bool, 6> isTrigger{false, false, true, false, false, true};
constexpr std::array<const char*, 6> axisNames{"left stick across", "left stick up and down",
                                               "left trigger",      "right stick across",
                                               "right stick up and down", "right trigger"};
/// How far towards each end of its range an axis must be swept to count as measured.
constexpr double sweptEnough = 0.5;
/// The measured ends are brought this far in, so a full push reaches full travel every time.
constexpr double endReach = 0.97;

std::int32_t along(std::int32_t from, std::int32_t to, double share) {
    return from + static_cast<std::int32_t>(std::lround(share * (static_cast<double>(to) - from)));
}

} // namespace

std::string calibrationKey(const std::string& name, const std::string& uniq) {
    return uniq.empty() ? name : name + " " + uniq;
}

std::int32_t stickValue(std::int32_t value, const AxisCalibration& axis, AxisRange to) {
    const std::int32_t centre = to.min + (to.max - to.min + 1) / 2;
    if (value >= axis.rest) {
        if (axis.high <= axis.rest) {
            return centre;
        }
        const double share = std::min(1.0, static_cast<double>(value - axis.rest) /
                                               (axis.high - axis.rest));
        return along(centre, to.max, share);
    }
    if (axis.low >= axis.rest) {
        return centre;
    }
    const double share =
        std::min(1.0, static_cast<double>(axis.rest - value) / (axis.rest - axis.low));
    return along(centre, to.min, share);
}

std::int32_t triggerValue(std::int32_t value, const AxisCalibration& axis, AxisRange to) {
    if (value <= axis.rest || axis.high <= axis.rest) {
        return to.min;
    }
    const double share =
        std::min(1.0, static_cast<double>(value - axis.rest) / (axis.high - axis.rest));
    return along(to.min, to.max, share);
}

void CalibrationCapture::still(const RawAxes& raw) {
    for (std::size_t axis = 0; axis < raw.value.size(); ++axis) {
        restSum_[axis] += raw.value[axis];
    }
    range_ = raw.range;
    ++restCount_;
}

void CalibrationCapture::sweep(const RawAxes& raw) {
    if (!swept_) {
        low_.fill(std::numeric_limits<std::int32_t>::max());
        high_.fill(std::numeric_limits<std::int32_t>::min());
        swept_ = true;
    }
    for (std::size_t axis = 0; axis < raw.value.size(); ++axis) {
        low_[axis] = std::min(low_[axis], raw.value[axis]);
        high_[axis] = std::max(high_[axis], raw.value[axis]);
    }
    range_ = raw.range;
}

std::optional<AxisCalibration> CalibrationCapture::measured(std::size_t axis) const {
    const AxisRange range = range_[axis];
    if (restCount_ == 0 || !swept_ || range.max <= range.min) {
        return std::nullopt;
    }
    const auto rest = static_cast<std::int32_t>(
        std::lround(static_cast<double>(restSum_[axis]) / static_cast<double>(restCount_)));
    const auto reached = [rest](std::int32_t end, std::int32_t limit) {
        return std::abs(static_cast<double>(end) - rest) >=
               sweptEnough * std::abs(static_cast<double>(limit) - rest);
    };
    if (!reached(high_[axis], range.max)) {
        return std::nullopt;
    }
    if (isTrigger[axis]) {
        return AxisCalibration{rest, rest, along(rest, high_[axis], endReach)};
    }
    if (!reached(low_[axis], range.min)) {
        return std::nullopt;
    }
    return AxisCalibration{along(rest, low_[axis], endReach), rest,
                           along(rest, high_[axis], endReach)};
}

PadCalibration CalibrationCapture::result() const {
    PadCalibration calibration;
    for (std::size_t axis = 0; axis < axisNames.size(); ++axis) {
        if (const std::optional<AxisCalibration> found = measured(axis)) {
            calibration.axes[static_cast<std::uint16_t>(axis)] = *found;
        }
    }
    return calibration;
}

std::vector<std::string> CalibrationCapture::missed() const {
    std::vector<std::string> names;
    for (std::size_t axis = 0; axis < axisNames.size(); ++axis) {
        if (range_[axis].max > range_[axis].min && !measured(axis)) {
            names.emplace_back(axisNames[axis]);
        }
    }
    return names;
}

} // namespace opensu::gamepad
