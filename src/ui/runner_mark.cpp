#include "runner_mark.hpp"

#include <cctype>

#include "library/own_games.hpp"

namespace opensu::ui {
namespace {

bool upper(char c) {
    return std::isupper(static_cast<unsigned char>(c)) != 0;
}

bool letter(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

RunnerMark shipped(std::optional<Icon> icon) {
    return RunnerMark{.icon = icon, .picture = {}, .letters = {}};
}

} // namespace

std::optional<RunnerMark> runnerMark(const library::Game& game) {
    switch (game.source) {
    case library::Source::Steam:
    case library::Source::Epic:
    case library::Source::Gog:
        return shipped(iconFor(game.source));
    case library::Source::Own:
        if (!library::own::nativeOwn(game)) {
            return shipped(Icon::Wine);
        }
        return shipped(game.appImage ? Icon::AppImage : Icon::Linux);
    case library::Source::Rom: {
        const library::EmulatorOption* runs = library::runningEmulator(game);
        if (runs == nullptr) {
            return std::nullopt;
        }
        if (runs->builtIn) {
            return shipped(Icon::Libretro);
        }
        return RunnerMark{
            .icon = std::nullopt, .picture = game.runnerIcon, .letters = initials(runs->name)};
    }
    case library::Source::Remote:
        break;
    }
    return std::nullopt;
}

std::string initials(std::string_view name) {
    std::string words;
    std::string capitals;
    std::string first;
    bool start = true;
    for (const char c : name) {
        if (!letter(c)) {
            start = true;
            continue;
        }
        if (start) {
            words += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (upper(c)) {
            capitals += c;
        }
        if (first.size() < 2) {
            first +=
                first.empty() ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
        }
        start = false;
    }
    if (words.size() >= 2) {
        return words.substr(0, 2);
    }
    if (capitals.size() >= 2) {
        return capitals.substr(0, 2);
    }
    return first;
}

} // namespace opensu::ui
