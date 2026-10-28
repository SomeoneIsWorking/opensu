#include "process_launcher.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cwctype>
#include <thread>

#include "command.hpp"
#include "lucent/log.h"
#include "win32.hpp"

namespace opensu::launch {
namespace {

using Clock = std::chrono::steady_clock;
using win32::Handle;
using win32::lastError;
using win32::wide;

/// How long the group's windows have to close before what is left is terminated, as the
/// systemd scope's TimeoutStopSec.
constexpr auto closeWait = std::chrono::seconds{5};
constexpr auto step = std::chrono::milliseconds{20};
/// The exit code a terminated group's processes get.
constexpr UINT terminatedCode = 1;

/// The variable's name in a "NAME=value" entry, upper-cased: Windows compares names ignoring case.
std::wstring nameOf(const std::wstring& entry) {
    // A name may start with '=' ("=C:=C:\\"), so the separator is searched after the first char.
    const std::size_t equals = entry.find(L'=', 1);
    std::wstring name = entry.substr(0, equals);
    std::ranges::transform(name, name.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towupper(c));
    });
    return name;
}

/// openSU's environment with each of `added` set over it, as CreateProcessW's sorted block.
std::wstring environmentBlock(const std::vector<std::string>& added) {
    std::vector<std::wstring> entries;
    if (wchar_t* own = GetEnvironmentStringsW(); own != nullptr) {
        for (const wchar_t* at = own; *at != L'\0'; at += std::wcslen(at) + 1) {
            entries.emplace_back(at);
        }
        FreeEnvironmentStringsW(own);
    }
    for (const std::string& entry : added) {
        std::wstring set = wide(entry);
        const std::wstring name = nameOf(set);
        std::erase_if(entries, [&name](const std::wstring& kept) {
            return nameOf(kept) == name;
        });
        entries.push_back(std::move(set));
    }
    std::ranges::sort(entries, [](const std::wstring& a, const std::wstring& b) {
        return nameOf(a) < nameOf(b);
    });
    std::wstring block;
    for (const std::wstring& entry : entries) {
        block += entry;
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

/// The processes in `job`.
std::vector<DWORD> processesIn(HANDLE job) {
    // The list's fixed part holds one id; room for more follows it.
    constexpr std::size_t room = 1024;
    std::vector<std::byte> buffer(sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) +
                                  room * sizeof(ULONG_PTR));
    auto* list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(buffer.data());
    if (QueryInformationJobObject(job, JobObjectBasicProcessIdList, list,
                                  static_cast<DWORD>(buffer.size()), nullptr) == 0 &&
        GetLastError() != ERROR_MORE_DATA) {
        return {};
    }
    std::vector<DWORD> ids;
    ids.reserve(list->NumberOfProcessIdsInList);
    for (DWORD i = 0; i < list->NumberOfProcessIdsInList; ++i) {
        ids.push_back(static_cast<DWORD>(list->ProcessIdList[i]));
    }
    return ids;
}

/// How many processes are left in `job`.
DWORD activeIn(HANDLE job) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
    if (QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &accounting,
                                  sizeof(accounting), nullptr) == 0) {
        return 0;
    }
    return accounting.ActiveProcesses;
}

/// Asks every top-level window of a process in `ids` to close, as its title bar's X does.
void closeWindowsOf(const std::vector<DWORD>& ids) {
    EnumWindows(
        [](HWND window, LPARAM context) -> BOOL {
            const auto& owners = *std::bit_cast<const std::vector<DWORD>*>(context);
            DWORD owner = 0;
            GetWindowThreadProcessId(window, &owner);
            if (std::ranges::find(owners, owner) != owners.end()) {
                PostMessageW(window, WM_CLOSE, 0, 0);
            }
            return TRUE;
        },
        std::bit_cast<LPARAM>(&ids));
}

} // namespace

/// The Job Object a launch runs in and the process it started. The job outlives the started
/// process and holds everything it starts; closing it ends them all.
struct ProcessLauncher::Group {
    Handle job;
    Handle process;

    /// True until the started process exits; records its exit code then.
    bool childRunning(int& exitStatus);
    /// Closes the group's windows, terminates what is left after closeWait and drops the job.
    void end(int& exitStatus);
};

ProcessLauncher::ProcessLauncher() : group_{std::make_unique<Group>()} {
}

ProcessLauncher::~ProcessLauncher() {
    stop();
}

bool ProcessLauncher::start(const std::string& group, const std::string& program,
                            const std::vector<std::string>& args, std::string& failure,
                            const std::vector<std::string>& environment,
                            const std::filesystem::path& workingDirectory,
                            const std::filesystem::path& output) {
    const std::lock_guard lock{mutex_};
    if (group_->job.valid()) {
        failure = "an instance is already running";
        return false;
    }
    Handle job{CreateJobObjectW(nullptr, nullptr)};
    if (!job.valid()) {
        failure = "could not create a job for " + programName(program);
        lucent::error("launch", "CreateJobObjectW failed: error {}", lastError());
        return false;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits,
                                sizeof(limits)) == 0) {
        failure = "could not create a job for " + programName(program);
        lucent::error("launch", "SetInformationJobObject failed: error {}", lastError());
        return false;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    Handle outputFile;
    BOOL inherit = FALSE;
    if (!output.empty()) {
        SECURITY_ATTRIBUTES inheritable{.nLength = sizeof(SECURITY_ATTRIBUTES),
                                        .lpSecurityDescriptor = nullptr,
                                        .bInheritHandle = TRUE};
        outputFile.reset(CreateFileW(output.wstring().c_str(), FILE_APPEND_DATA,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!outputFile.valid()) {
            failure = "cannot write " + output.string() + ": error " + lastError();
            return false;
        }
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = outputFile.get();
        startup.hStdError = outputFile.get();
        inherit = TRUE;
    }

    std::wstring line = win32::commandLine(program, args);
    std::wstring block = environmentBlock(environment);
    const std::wstring directory = workingDirectory.wstring();
    PROCESS_INFORMATION info{};
    if (CreateProcessW(nullptr, line.data(), nullptr, nullptr, inherit,
                       CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, block.data(),
                       directory.empty() ? nullptr : directory.c_str(), &startup, &info) == 0) {
        lucent::warn("launch", "cannot run {}: error {}", program, lastError());
        failure = "could not start " + programName(program);
        return false;
    }
    Handle process{info.hProcess};
    const Handle thread{info.hThread};
    if (AssignProcessToJobObject(job.get(), process.get()) == 0) {
        lucent::error("launch", "cannot put {} in its job: error {}", program, lastError());
        TerminateProcess(process.get(), terminatedCode);
        failure = "could not create a job for " + programName(program);
        return false;
    }
    ResumeThread(thread.get());
    group_->job.reset(job.release());
    group_->process.reset(process.release());
    exitStatus_ = -1;
    lucent::info("launch", "{} started in job {} (pid {})", program, group, info.dwProcessId);
    return true;
}

bool ProcessLauncher::running() {
    const std::lock_guard lock{mutex_};
    if (!group_->job.valid()) {
        return false;
    }
    group_->childRunning(exitStatus_);
    if (activeIn(group_->job.get()) > 0) {
        return true;
    }
    group_->job.close();
    return false;
}

bool ProcessLauncher::startedRunning() {
    const std::lock_guard lock{mutex_};
    return group_->childRunning(exitStatus_);
}

void ProcessLauncher::stop() {
    const std::lock_guard lock{mutex_};
    group_->end(exitStatus_);
}

int ProcessLauncher::exitStatus() const {
    const std::lock_guard lock{mutex_};
    return exitStatus_;
}

/// Caller holds the launcher's mutex.
bool ProcessLauncher::Group::childRunning(int& exitStatus) {
    if (!process.valid()) {
        return false;
    }
    if (WaitForSingleObject(process.get(), 0) == WAIT_TIMEOUT) {
        return true;
    }
    DWORD code = 0;
    if (GetExitCodeProcess(process.get(), &code) != 0) {
        exitStatus = static_cast<int>(code);
    }
    process.close();
    return false;
}

/// Caller holds the launcher's mutex.
void ProcessLauncher::Group::end(int& exitStatus) {
    if (!job.valid()) {
        childRunning(exitStatus);
        return;
    }
    lucent::info("launch", "closing the job's windows");
    closeWindowsOf(processesIn(job.get()));
    const Clock::time_point until = Clock::now() + closeWait;
    while (activeIn(job.get()) > 0 && Clock::now() < until) {
        std::this_thread::sleep_for(step);
    }
    if (activeIn(job.get()) > 0) {
        lucent::warn("launch", "the job outlived its windows; terminating it");
        TerminateJobObject(job.get(), terminatedCode);
    }
    if (process.valid()) {
        WaitForSingleObject(process.get(), INFINITE);
    }
    childRunning(exitStatus);
    job.close();
}

} // namespace opensu::launch
