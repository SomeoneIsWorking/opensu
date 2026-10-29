// sdl_log — SDL's own log lines, sent through Lucent while an `SdlLog` lives.
#pragma once

namespace opensu::app {

/// Routes SDL's log output into Lucent, GPU warnings included (why a GPU driver was refused), and
/// gives SDL its default output back when it goes.
class SdlLog {
  public:
    SdlLog();
    ~SdlLog();
    SdlLog(const SdlLog&) = delete;
    SdlLog& operator=(const SdlLog&) = delete;
    SdlLog(SdlLog&&) = delete;
    SdlLog& operator=(SdlLog&&) = delete;
};

} // namespace opensu::app
