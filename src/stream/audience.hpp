// audience — the guests one host session sends to: each guest's `HostStream`, under one lock. The
// video and the sound send from their capture threads while the loop adds and removes guests, so
// both go through `visit`, and a stream taken out with `remove` is never sent to again once that
// returns.
#pragma once

#include <functional>
#include <mutex>
#include <span>
#include <vector>

#include "host_stream.hpp"

namespace opensu::stream {

class Audience {
  public:
    /// Sends to `stream` from the next frame on; it outlives its time here.
    void add(HostStream& stream);
    /// Stops sending to `stream`; no visit holds it once this returns.
    void remove(const HostStream& stream);
    /// Calls `visit` with the streams whose guest joined and whose session is not over, under the
    /// lock; any thread.
    void visit(const std::function<void(std::span<HostStream* const>)>& visit) const;

  private:
    mutable std::mutex mutex_;
    std::vector<HostStream*> streams_;
};

} // namespace opensu::stream
