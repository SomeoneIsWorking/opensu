#include "child_process.hpp"

#include <array>
#include <thread>

#include "lucent/log.h"
#include "process_id.hpp"
#include "win32.hpp"

namespace opensu::launch {
namespace {

namespace fs = std::filesystem;
using win32::commandLine;
using win32::Handle;
using win32::lastError;

constexpr auto step = std::chrono::milliseconds{5};
/// The extensions CreateProcessW runs, in the order it tries them.
constexpr std::array<std::wstring_view, 2> runnableExtensions{L".exe", L".com"};

/// The null device, inheritable, for a child's discarded stream.
HANDLE openNull(DWORD access) {
    SECURITY_ATTRIBUTES inherit{.nLength = sizeof(SECURITY_ATTRIBUTES),
                                .lpSecurityDescriptor = nullptr,
                                .bInheritHandle = TRUE};
    return CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
}

/// A pipe whose `inherited` end the child gets and whose other end stays with openSU.
bool makePipe(Handle& reader, Handle& writer, bool childReads) {
    SECURITY_ATTRIBUTES inherit{.nLength = sizeof(SECURITY_ATTRIBUTES),
                                .lpSecurityDescriptor = nullptr,
                                .bInheritHandle = TRUE};
    if (CreatePipe(reader.put(), writer.put(), &inherit, 0) == 0) {
        return false;
    }
    HANDLE ours = childReads ? writer.get() : reader.get();
    return SetHandleInformation(ours, HANDLE_FLAG_INHERIT, 0) != 0;
}

/// A started child: its process, and the Job Object that owns what it starts.
struct Started {
    Handle process;
    Handle job;
};

/// The handles a child gets as its stdin, stdout and stderr.
struct StandardHandles {
    HANDLE input;
    HANDLE output;
    HANDLE errors;
};

/// Starts `program` with the given standard handles, suspended until it is in its job.
bool start(const std::string& program, const std::vector<std::string>& args,
           const StandardHandles& standard, Started& started) {
    started.job.reset(CreateJobObjectW(nullptr, nullptr));
    if (started.job.get() == nullptr) {
        lucent::warn("launch", "cannot make a job for {}: error {}", program, lastError());
        return false;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(started.job.get(), JobObjectExtendedLimitInformation, &limits,
                            sizeof(limits));

    std::wstring line = commandLine(program, args);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = standard.input;
    startup.hStdOutput = standard.output;
    startup.hStdError = standard.errors;
    PROCESS_INFORMATION info{};
    if (CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE,
                       CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr,
                       nullptr, &startup, &info) == 0) {
        lucent::warn("launch", "cannot run {}: error {}", program, lastError());
        return false;
    }
    started.process.reset(info.hProcess);
    const Handle thread{info.hThread};
    if (AssignProcessToJobObject(started.job.get(), info.hProcess) == 0) {
        lucent::warn("launch", "cannot put {} in its job: error {}", program, lastError());
        TerminateProcess(info.hProcess, 1);
        return false;
    }
    ResumeThread(thread.get());
    return true;
}

std::optional<int> exitCodeOf(HANDLE process) {
    DWORD code = 0;
    if (GetExitCodeProcess(process, &code) == 0) {
        return std::nullopt;
    }
    return static_cast<int>(code);
}

bool writeAll(HANDLE pipe, std::string_view data) {
    while (!data.empty()) {
        DWORD put = 0;
        if (WriteFile(pipe, data.data(), static_cast<DWORD>(data.size()), &put, nullptr) == 0 ||
            put == 0) {
            return false;
        }
        data.remove_prefix(put);
    }
    return true;
}

} // namespace

ProcessId ownProcessId() noexcept {
    return static_cast<ProcessId>(GetCurrentProcessId());
}

bool child::namesFile(const std::string& program) {
    return program.find_first_of("/\\:") != std::string::npos;
}

std::optional<fs::path> child::executableAt(const fs::path& candidate) {
    std::error_code ec;
    for (const std::wstring_view extension : runnableExtensions) {
        if (candidate.extension().wstring() == extension && fs::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    for (const std::wstring_view extension : runnableExtensions) {
        fs::path named = candidate;
        named += extension;
        if (fs::is_regular_file(named, ec)) {
            return named;
        }
    }
    return std::nullopt;
}

std::optional<int> child::run(const std::string& program, const std::vector<std::string>& args,
                              std::chrono::milliseconds timeout) {
    const Handle input{openNull(GENERIC_READ)};
    const Handle output{openNull(GENERIC_WRITE)};
    Started started;
    if (!start(program, args,
               {.input = input.get(), .output = output.get(), .errors = output.get()}, started)) {
        return std::nullopt;
    }
    if (WaitForSingleObject(started.process.get(), static_cast<DWORD>(timeout.count())) !=
        WAIT_OBJECT_0) {
        lucent::error("launch", "{} did not finish in time; killing it", program);
        TerminateJobObject(started.job.get(), 1);
        WaitForSingleObject(started.process.get(), INFINITE);
        return std::nullopt;
    }
    return exitCodeOf(started.process.get());
}

std::optional<int> child::stream(const std::string& program, const std::vector<std::string>& args,
                                 const std::function<void(std::string_view)>& onLine,
                                 const std::stop_token& stop, CaptureErrors errors,
                                 const std::optional<std::string_view>& stdinLine) {
    Handle reader;
    Handle writer;
    if (!makePipe(reader, writer, false)) {
        lucent::warn("launch", "cannot make a pipe for {}: error {}", program, lastError());
        return std::nullopt;
    }
    Handle stdinReader;
    Handle stdinWriter;
    if (stdinLine) {
        if (!makePipe(stdinReader, stdinWriter, true)) {
            lucent::warn("launch", "cannot make a pipe for {}: error {}", program, lastError());
            return std::nullopt;
        }
    } else {
        stdinReader.reset(openNull(GENERIC_READ));
    }
    Handle discarded;
    if (errors == CaptureErrors::Discarded) {
        discarded.reset(openNull(GENERIC_WRITE));
    }
    Started started;
    const HANDLE errorsTo = errors == CaptureErrors::Merged ? writer.get() : discarded.get();
    if (!start(program, args,
               {.input = stdinReader.get(), .output = writer.get(), .errors = errorsTo}, started)) {
        return std::nullopt;
    }
    writer.close();
    stdinReader.close();
    if (stdinLine) {
        // A child that has already gone is found out by its status below.
        static_cast<void>(writeAll(stdinWriter.get(), *stdinLine) &&
                          writeAll(stdinWriter.get(), "\n"));
        stdinWriter.close();
    }

    std::string pending;
    std::array<char, 4096> chunk{};
    for (;;) {
        if (stop.stop_requested()) {
            TerminateJobObject(started.job.get(), 1);
            WaitForSingleObject(started.process.get(), INFINITE);
            return std::nullopt;
        }
        DWORD available = 0;
        // Fails once every writer is gone: the child and whatever inherited its output.
        if (PeekNamedPipe(reader.get(), nullptr, 0, nullptr, &available, nullptr) == 0) {
            break;
        }
        if (available == 0) {
            std::this_thread::sleep_for(step);
            continue;
        }
        DWORD got = 0;
        const DWORD want = std::min<DWORD>(available, static_cast<DWORD>(chunk.size()));
        if (ReadFile(reader.get(), chunk.data(), want, &got, nullptr) == 0 || got == 0) {
            break;
        }
        pending.append(chunk.data(), got);
        for (std::size_t end = pending.find('\n'); end != std::string::npos;
             end = pending.find('\n')) {
            std::string_view text = std::string_view{pending}.substr(0, end);
            if (text.ends_with('\r')) {
                text.remove_suffix(1);
            }
            onLine(text);
            pending.erase(0, end + 1);
        }
    }
    if (!pending.empty()) {
        onLine(pending);
    }
    WaitForSingleObject(started.process.get(), INFINITE);
    return exitCodeOf(started.process.get());
}

} // namespace opensu::launch
