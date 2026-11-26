// stream_meter_painter — the remote play stream's meter over its picture, as Steam shows its
// stream's: a small dark pill in the top right corner with a dot in the stream's grade (green,
// amber, red) and one line of its latency, round trip, bitrate, frames a second and frames lost.
// The line is written when the reading changes, once a second at most; drawing it reuses its label.
#pragma once

#include <string>

#include "gpu2d/ui/typesetter.hpp"
#include "paint.hpp"
#include "stream/stream_meter.hpp"

namespace opensu::ui {

/// A reading as the meter shows it.
struct StreamMeterLine {
    stream::StreamReading reading;
    std::string text;
};

/// `reading` as one line: "18 ms · ping 3 ms · 24.6 Mb/s · 60 fps", with " · 2 lost" when any were.
[[nodiscard]] StreamMeterLine streamMeterLine(const stream::StreamReading& reading);

class StreamMeterPainter {
  public:
    explicit StreamMeterPainter(gpu2d::ui::Typesetter& text) noexcept : text_{text} {
    }

    /// Draws `line` in the top right corner of `area`, at `dp` pixels per dp.
    void paint(Canvas& canvas, const StreamMeterLine& line, const gpu2d::Rect& area,
               float dp) const;

  private:
    gpu2d::ui::Typesetter& text_;
};

} // namespace opensu::ui
