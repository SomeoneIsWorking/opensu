#include "win32.hpp"

namespace opensu::launch::win32 {
namespace {

/// `argument` quoted so CommandLineToArgvW reads it back unchanged.
void appendQuoted(std::wstring& line, const std::wstring& argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        line += argument;
        return;
    }
    line.push_back(L'"');
    for (auto it = argument.begin();; ++it) {
        std::size_t backslashes = 0;
        while (it != argument.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }
        if (it == argument.end()) {
            line.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            line.append(backslashes * 2 + 1, L'\\');
        } else {
            line.append(backslashes, L'\\');
        }
        line.push_back(*it);
    }
    line.push_back(L'"');
}

} // namespace

std::wstring commandLine(const std::string& program, const std::vector<std::string>& args) {
    std::wstring line;
    appendQuoted(line, wide(program));
    for (const std::string& arg : args) {
        line.push_back(L' ');
        appendQuoted(line, wide(arg));
    }
    return line;
}

} // namespace opensu::launch::win32
