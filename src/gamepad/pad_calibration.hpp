// pad_calibration — a pad's own measured axes: where each stick axis rests and how far it reaches
// each way, and where each trigger rests and ends, read as the virtual pad's whole ranges. A worn
// or hall-effect stick that rests off centre or stops short of its range plays as a true one.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace opensu::gamepad {

/// An absolute axis' range as the device reports it.
struct AxisRange {
    std::int32_t min{};
    std::int32_t max{};

    friend bool operator==(const AxisRange&, const AxisRange&) = default;
};

/// One axis as measured: its two ends and where it rests.
struct AxisCalibration {
    std::int32_t low{};
    std::int32_t rest{};
    std::int32_t high{};

    friend bool operator==(const AxisCalibration&, const AxisCalibration&) = default;
};

/// A pad's measured axes, by evdev code (`ABS_X` to `ABS_RZ`); an axis missing reads as reported.
struct PadCalibration {
    std::map<std::uint16_t, AxisCalibration> axes;

    friend bool operator==(const PadCalibration&, const PadCalibration&) = default;
};

/// The calibrated pads, by `calibrationKey`.
using PadCalibrations = std::map<std::string, PadCalibration>;

/// Which pad a calibration is for, across reconnects: its name and unique id.
[[nodiscard]] std::string calibrationKey(const std::string& name, const std::string& uniq);

/// A stick axis' `value` on `to`: its rest is the centre and each end the range's end.
[[nodiscard]] std::int32_t stickValue(std::int32_t value, const AxisCalibration& axis,
                                      AxisRange to);
/// A trigger's `value` on `to`: its rest is `to.min` and its end `to.max`.
[[nodiscard]] std::int32_t triggerValue(std::int32_t value, const AxisCalibration& axis,
                                        AxisRange to);

/// The axes a calibration measures, `ABS_X` to `ABS_RZ`, as the pad reads them before any
/// calibration; an axis the pad lacks has an empty range.
struct RawAxes {
    std::array<std::int32_t, 6> value{};
    std::array<AxisRange, 6> range{};

    friend bool operator==(const RawAxes&, const RawAxes&) = default;
};

/// Measures a pad from its raw readings: the rests while it is left alone, then the ends while the
/// sticks go round their edges and the triggers are pulled.
class CalibrationCapture {
  public:
    void still(const RawAxes& raw);
    void sweep(const RawAxes& raw);
    /// The axes measured; an axis not swept at least halfway to each end is left out.
    [[nodiscard]] PadCalibration result() const;
    /// The names of the axes left out of `result`.
    [[nodiscard]] std::vector<std::string> missed() const;

  private:
    [[nodiscard]] std::optional<AxisCalibration> measured(std::size_t axis) const;

    std::array<std::int64_t, 6> restSum_{};
    std::size_t restCount_{0};
    std::array<std::int32_t, 6> low_{};
    std::array<std::int32_t, 6> high_{};
    std::array<AxisRange, 6> range_{};
    bool swept_{false};
};

} // namespace opensu::gamepad
