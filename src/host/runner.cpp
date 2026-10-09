#include "runner.hpp"

#include <string_view>
#include <thread>

#include "lucent/log.h"

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

} // namespace

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
