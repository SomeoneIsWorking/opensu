#include "process_table.hpp"

// Windows.h comes first: the headers after it build on its types.
#include "win32.hpp"

#include <shellapi.h>
#include <tlhelp32.h>
#include <winternl.h>

namespace opensu::launch::table {
namespace {

using win32::Handle;

/// NtQueryInformationProcess's class for a process's command line (Windows 8.1 and later).
constexpr ULONG commandLineInformation = 60;

/// NtQueryInformationProcess, its class taken as the number it is.
using QueryProcess = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);

/// Each process's entry in a fresh snapshot, until `visit` returns false.
template <typename Visit> void eachProcess(Visit visit) {
    const Handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot.valid()) {
        return;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Process32FirstW(snapshot.get(), &entry); more != 0;
         more = Process32NextW(snapshot.get(), &entry)) {
        if (!visit(entry)) {
            return;
        }
    }
}

/// `pid`'s command line as one string, or empty when it cannot be read.
std::wstring rawCommandLine(ProcessId pid) {
    const Handle process{
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid))};
    if (!process.valid()) {
        return {};
    }
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return {};
    }
    const auto query =
        reinterpret_cast<QueryProcess>(GetProcAddress(ntdll, "NtQueryInformationProcess"));
    if (query == nullptr) {
        return {};
    }
    ULONG size = 0;
    query(process.get(), commandLineInformation, nullptr, 0, &size);
    if (size < sizeof(UNICODE_STRING)) {
        return {};
    }
    std::vector<std::byte> buffer(size);
    if (query(process.get(), commandLineInformation, buffer.data(), size, &size) < 0) {
        return {};
    }
    const auto* text = reinterpret_cast<const UNICODE_STRING*>(buffer.data());
    return {text->Buffer, text->Length / sizeof(wchar_t)};
}

} // namespace

bool available() {
    return true;
}

std::vector<ProcessId> pids() {
    std::vector<ProcessId> found;
    eachProcess([&found](const PROCESSENTRY32W& entry) {
        found.push_back(static_cast<ProcessId>(entry.th32ProcessID));
        return true;
    });
    return found;
}

ProcessId parentOf(ProcessId pid) {
    ProcessId parent = -1;
    eachProcess([pid, &parent](const PROCESSENTRY32W& entry) {
        if (static_cast<ProcessId>(entry.th32ProcessID) != pid) {
            return true;
        }
        parent = static_cast<ProcessId>(entry.th32ParentProcessID);
        return false;
    });
    return parent;
}

/// Windows keeps one command line; it is split as the C runtime splits it for main().
std::string commandLine(ProcessId pid) {
    const std::wstring raw = rawCommandLine(pid);
    if (raw.empty()) {
        return {};
    }
    int count = 0;
    wchar_t** args = CommandLineToArgvW(raw.c_str(), &count);
    if (args == nullptr) {
        return {};
    }
    std::string joined;
    for (int i = 0; i < count; ++i) {
        joined += win32::narrow(args[i]);
        joined.push_back('\0');
    }
    LocalFree(static_cast<HLOCAL>(args));
    return joined;
}

bool kill(ProcessId pid) {
    const Handle process{OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid))};
    return process.valid() && TerminateProcess(process.get(), 1) != 0;
}

} // namespace opensu::launch::table
