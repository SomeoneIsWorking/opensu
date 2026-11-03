#include "runner.hpp"

#include <string_view>
#include <thread>

#include "lucent/log.h"
#include "lucent/text.h"

namespace opensu::host {
namespace {

class StreamingHolder final : public Holder {
  public:
    StreamingHolder(std::string program, std::vector<std::string> args)
        : thread_{
              [program = std::move(program), args = std::move(args)](const std::stop_token& stop) {
                  const std::optional<int> status = launch::runStreaming(
                      program, args,
                      [](std::string_view) {
                      },
                      stop);
                  if (!status && !stop.stop_requested()) {
                      lucent::warn("host", "{} could not be held", program);
                  }
              }} {
    }
    ~StreamingHolder() override {
        thread_.request_stop();
    }
    StreamingHolder(const StreamingHolder&) = delete;
    StreamingHolder& operator=(const StreamingHolder&) = delete;
    StreamingHolder(StreamingHolder&&) = delete;
    StreamingHolder& operator=(StreamingHolder&&) = delete;

  private:
    std::jthread thread_;
};

/// The first line of `output`, trimmed.
std::string firstLine(std::string_view output) {
    const std::string_view text = lucent::text::trim(output);
    return std::string{lucent::text::trim(text.substr(0, text.find('\n')))};
}

} // namespace

std::string refusalOf(const std::string& program, const std::optional<launch::Captured>& out,
                      const std::string& what) {
    if (!out) {
        return program + " could not be run";
    }
    if (out->status == 0) {
        return {};
    }
    const std::string reason = firstLine(out->output);
    return reason.empty() ? program + " refused to " + what : reason;
}

Runner systemRunner() {
    return [](const std::string& program, const std::vector<std::string>& args) {
        return launch::runCaptured(program, args, launch::CaptureErrors::Merged);
    };
}

Spawner systemSpawner(std::vector<std::filesystem::path> executablePath) {
    return [executablePath = std::move(executablePath)](
               const std::string& program,
               const std::vector<std::string>& args) -> std::unique_ptr<Holder> {
        const std::filesystem::path found = launch::resolveExecutable(program, executablePath);
        if (found.empty()) {
            return nullptr;
        }
        return std::make_unique<StreamingHolder>(found.string(), args);
    };
}

} // namespace opensu::host
