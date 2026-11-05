#include "control_commands.hpp"

#include <nlohmann/json.hpp>

#include "gpu2d/image/png_encoder.hpp"
#include "lucent/log.h"

namespace opensu::retro {
namespace {

ControlAnswer answerOf(const StateOutcome& outcome) {
    switch (outcome.result) {
    case StateResult::Done:
        return {};
    case StateResult::Refused:
        return {409, outcome.reason};
    case StateResult::Missing:
        return {404, outcome.reason};
    case StateResult::FileFailed:
        break;
    }
    return {500, outcome.reason};
}

bool changesTheGame(ControlCommand command) {
    return command == ControlCommand::LoadState || command == ControlCommand::Reset ||
           command == ControlCommand::Option || command == ControlCommand::Suspend;
}

std::string optionsJson(const CoreOptions& options) {
    nlohmann::json list = nlohmann::json::array();
    for (const CoreOptions::Option& option : options.options()) {
        list.push_back(nlohmann::json{{"key", option.key},
                                      {"label", option.label},
                                      {"info", option.info},
                                      {"values", option.values},
                                      {"labels", option.valueLabels},
                                      {"default", option.fallback},
                                      {"value", option.current}});
    }
    return list.dump();
}

ControlAnswer changeVideo(Presenter& presenter, const settings::VideoSettings& video) {
    try {
        presenter.change(video);
    } catch (const ShaderError& failure) {
        lucent::error("retro", "{}", failure.what());
        return {409, failure.what()};
    }
    lucent::info("retro", "video now {}", settings::videoJson(video));
    return {};
}

} // namespace

std::string framePng(Presenter& presenter, std::string& error) {
    Readback frame;
    try {
        frame = presenter.readBack();
    } catch (const VulkanError& failure) {
        error = failure.what();
        return {};
    }
    if (frame.rgba.empty()) {
        error = "no frame was shown";
        return {};
    }
    return gpu2d::image::encodePng(frame.rgba, frame.width, frame.height, error);
}

ControlAnswer ControlCommands::suspend() {
    // The state first: a sync reading between the two finds the old save and leaves the state out.
    ControlAnswer answer = answerOf(parts_.resume.suspend(parts_.core));
    if (answer.status != 200) {
        return answer;
    }
    if (!parts_.saves.keep(parts_.core.saveRam())) {
        return {500, "the game's save was not written"};
    }
    suspended_ = true;
    lucent::info("retro", "saved and waiting to be closed");
    return answer;
}

ControlAnswer ControlCommands::perform(const ControlRequest& request) {
    if (parts_.netplay && changesTheGame(request.command)) {
        return {409, "not while playing together"};
    }
    ControlAnswer answer;
    switch (request.command) {
    case ControlCommand::SaveState:
        answer = answerOf(parts_.state.save(parts_.core));
        break;
    case ControlCommand::LoadState:
        answer = answerOf(parts_.state.load(parts_.core));
        break;
    case ControlCommand::Reset:
        parts_.core.reset();
        lucent::info("retro", "game reset");
        break;
    case ControlCommand::Video:
        answer = changeVideo(parts_.presenter, request.video);
        break;
    case ControlCommand::Options:
        answer.json = optionsJson(parts_.options);
        break;
    case ControlCommand::Option:
        if (!parts_.options.set(request.key, request.value)) {
            answer = {409, "no such core option or value"};
            break;
        }
        answer.json = optionsJson(parts_.options);
        break;
    case ControlCommand::Suspend:
        answer = suspend();
        break;
    case ControlCommand::Frame: {
        if (!parts_.hidden) {
            answer = {409, "the frame is read back in hidden runs only"};
            break;
        }
        std::string error;
        answer.png = framePng(parts_.presenter, error);
        if (answer.png.empty()) {
            answer = {500, error};
        }
        break;
    }
    }
    answer.hasState = parts_.state.exists();
    return answer;
}

} // namespace opensu::retro
