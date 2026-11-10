#include "qt_config_ini.hpp"

namespace opensu::library {
namespace {

constexpr std::string_view sdlEngine{"engine:sdl"};

/// Each `key:value` pair of a param package, in order.
template <typename Visit> void forEachPair(std::string_view binding, Visit visit) {
    std::size_t at = 0;
    while (at <= binding.size()) {
        std::size_t end = binding.find(',', at);
        end = end == std::string_view::npos ? binding.size() : end;
        visit(binding.substr(at, end - at));
        at = end + 1;
    }
}

} // namespace

std::string withQtSetting(std::string_view ini, const IniKey& key, std::string_view value) {
    const std::string fallback = std::string{key.key} + "\\default";
    return withIniValue(withIniValue(ini, {key.section, fallback}, "false"), key, value);
}

std::string qtUnquoted(std::string_view value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return std::string{value};
}

std::string qtQuoted(std::string_view value) {
    return "\"" + std::string{value} + "\"";
}

bool isSdlBinding(std::string_view binding) {
    bool sdl = false;
    forEachPair(binding, [&sdl](std::string_view pair) {
        sdl = sdl || pair == sdlEngine;
    });
    return sdl;
}

std::string withSdlPad(std::string_view binding, std::string_view guid, int port) {
    if (!isSdlBinding(binding)) {
        return std::string{binding};
    }
    std::string out;
    forEachPair(binding, [&](std::string_view pair) {
        if (!out.empty()) {
            out += ',';
        }
        if (pair.starts_with("guid:")) {
            out += "guid:";
            out += guid;
        } else if (pair.starts_with("port:")) {
            out += "port:" + std::to_string(port);
        } else {
            out += pair;
        }
    });
    return out;
}

} // namespace opensu::library
