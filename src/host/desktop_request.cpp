#include "desktop_request.hpp"

#include <fstream>
#include <utility>

#include <unistd.h>

#include "launch/command.hpp"

namespace opensu::host {

DesktopRequest::DesktopRequest(std::filesystem::path file) : file_{std::move(file)} {
}

std::string DesktopRequest::write() const {
    std::error_code error;
    std::filesystem::create_directories(file_.parent_path(), error);
    if (error) {
        return "cannot create " + file_.parent_path().string() + ": " + error.message();
    }
    std::ofstream out{file_, std::ios::trunc};
    out << "desktop\n";
    out.close();
    return out ? std::string{} : "cannot write " + file_.string();
}

bool DesktopRequest::consume() const {
    std::error_code error;
    return std::filesystem::remove(file_, error);
}

std::string DesktopRequest::enterDesktop(const std::vector<std::filesystem::path>& path) {
    const std::filesystem::path program = launch::resolveExecutable(desktopProgram, path);
    if (program.empty()) {
        return std::string{desktopProgram} + " is not on PATH";
    }
    const std::string file = program.string();
    execl(file.c_str(), file.c_str(), static_cast<char*>(nullptr));
    return "cannot run " + file;
}

} // namespace opensu::host
