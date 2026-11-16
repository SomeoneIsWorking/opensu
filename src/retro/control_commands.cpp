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
           command == ControlCommand::Option || command == ControlCommand::Suspend ||
           command == ControlCommand::Hold;
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
                                      {"value", option.current},
                                      {"category", option.category}});
    }
    return list.dump();
}

ControlAnswer changeVideo(Presenter& presenter, ScreenTurn& screen,
                          const settings::VideoSettings& video) {
    try {
        presenter.change(video);
        screen.set(video.turned);
    } catch (const ShaderError& failure) {
        lucent::error("retro", "{}", failure.what());
        return {409, failure.what()};
    }
    lucent::info("retro", "video now {}", settings::videoJson(video));
    return {};
}

} // namespace

namespace {

/// The last image shown; empty, with `error` set, when there is none.
Readback lastFrame(Presenter& presenter, std::string& error) {
    Readback frame;
    try {
        frame = presenter.readBack();
    } catch (const VulkanError& failure) {
        error = failure.what();
        return {};
    }
    if (frame.rgba.empty()) {
        error = "no frame was shown";
    }
    return frame;
}

} // namespace

std::string framePng(Presenter& presenter, std::string& error) {
    const Readback frame = lastFrame(presenter, error);
    if (frame.rgba.empty()) {
        return {};
    }
    return gpu2d::image::encodePng(frame.rgba, frame.width, frame.height, error);
}

ControlAnswer ControlCommands::startRecord(const ControlRequest& request) {
    if (record_) {
        return {409, "a record runs already"};
    }
    std::string error;
    Readback first = lastFrame(parts_.presenter, error);
    if (first.rgba.empty()) {
        return {500, error};
    }
    using Clock = fileio::FrameRecord::Clock;
    const Clock::time_point start = Clock::now();
    const auto startedMicros = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
    record_ = std::make_unique<fileio::FrameRecord>(
        request.path, request.fps,
        fileio::FrameRecord::Size{.width = static_cast<int>(first.width),
                                  .height = static_cast<int>(first.height)},
        start);
    record_->add(std::move(first.rgba), start);
    lucent::info("retro", "recording {}x{} at {} a second to {}", first.width, first.height,
                 request.fps, request.path);
    ControlAnswer answer;
    answer.json = nlohmann::json{{"width", first.width},
                                 {"height", first.height},
                                 {"fps", request.fps},
                                 {"startedMicros", startedMicros}}
                      .dump();
    return answer;
}

ControlAnswer ControlCommands::stopRecord() {
    if (!record_) {
        return {409, "no record runs"};
    }
    const fileio::FrameRecord::Totals totals = record_->finish(fileio::FrameRecord::Clock::now());
    record_.reset();
    lucent::info("retro", "record ended: {} frames, {} repeated", totals.frames, totals.repeated);
    ControlAnswer answer;
    answer.json = nlohmann::json{{"frames", totals.frames},
                                 {"repeated", totals.repeated},
                                 {"error", totals.error}}
                      .dump();
    return answer;
}

void ControlCommands::recordShown() {
    if (!record_) {
        return;
    }
    const fileio::FrameRecord::Clock::time_point now = fileio::FrameRecord::Clock::now();
    if (!record_->due(now)) {
        return;
    }
    std::string error;
    Readback frame = lastFrame(parts_.presenter, error);
    record_->add(std::move(frame.rgba), now);
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
        answer = changeVideo(parts_.presenter, parts_.screen, request.video);
        break;
    case ControlCommand::Turn:
        parts_.screen.toggle();
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
    case ControlCommand::Hold:
        held_ = true;
        break;
    case ControlCommand::Release:
        held_ = false;
        break;
    case ControlCommand::Quit:
        quitting_ = true;
        lucent::info("retro", "the game quits on the control channel's word");
        break;
    case ControlCommand::Frame: {
        std::string error;
        answer.png = framePng(parts_.presenter, error);
        if (answer.png.empty()) {
            answer = {500, error};
        }
        break;
    }
    case ControlCommand::Record:
        answer = startRecord(request);
        break;
    case ControlCommand::RecordStop:
        answer = stopRecord();
        break;
    case ControlCommand::FrameRgba: {
        std::string error;
        const Readback frame = lastFrame(parts_.presenter, error);
        if (frame.rgba.empty()) {
            answer = {500, error};
            break;
        }
        answer.rgba.assign(frame.rgba.begin(), frame.rgba.end());
        answer.width = frame.width;
        answer.height = frame.height;
        break;
    }
    }
    answer.hasState = parts_.state.exists();
    return answer;
}

} // namespace opensu::retro
