#include "command.hpp"

#include "child_process.hpp"

namespace opensu::launch {

namespace fs = std::filesystem;

std::string programName(const std::string& program) {
    return fs::path{program}.filename().string();
}

fs::path resolveExecutable(const std::string& program, const std::vector<fs::path>& path) {
    if (program.empty()) {
        return {};
    }
    if (child::namesFile(program)) {
        return child::executableAt(program).value_or(fs::path{});
    }
    for (const fs::path& dir : path) {
        if (std::optional<fs::path> found = child::executableAt(dir / program)) {
            return *found;
        }
    }
    return {};
}

std::optional<int> runCommand(const std::string& program, const std::vector<std::string>& args,
                              std::chrono::milliseconds timeout) {
    return child::run(program, args, timeout);
}

std::optional<int> runStreaming(const std::string& program, const std::vector<std::string>& args,
                                const std::function<void(std::string_view)>& onLine,
                                const std::stop_token& stop) {
    return child::stream(program, args, onLine, stop, CaptureErrors::Merged, std::nullopt);
}

std::optional<Captured> runCaptured(const std::string& program,
                                    const std::vector<std::string>& args, CaptureErrors errors) {
    Captured captured;
    const std::optional<int> status = child::stream(
        program, args,
        [&captured](std::string_view line) {
            captured.output.append(line);
            captured.output.push_back('\n');
        },
        std::stop_token{}, errors, std::nullopt);
    if (!status) {
        return std::nullopt;
    }
    captured.status = *status;
    return captured;
}

std::optional<Captured> runCapturedWithLine(const std::string& program,
                                            const std::vector<std::string>& args,
                                            std::string_view line) {
    Captured captured;
    const std::optional<int> status = child::stream(
        program, args,
        [&captured](std::string_view out) {
            captured.output.append(out);
            captured.output.push_back('\n');
        },
        std::stop_token{}, CaptureErrors::Merged, line);
    if (!status) {
        return std::nullopt;
    }
    captured.status = *status;
    return captured;
}

} // namespace opensu::launch
