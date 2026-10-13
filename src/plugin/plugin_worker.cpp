#include "plugin_worker.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>

#include "lucent/log.h"
#include "sources.hpp"

namespace opensu::plugin {
namespace {

/// An error and its traceback on one line, for the log.
std::string oneLine(const std::string& text) {
    std::string out;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        std::string_view line = std::string_view{text}.substr(start, end - start);
        while (line.starts_with('\t') || line.starts_with(' ')) {
            line.remove_prefix(1);
        }
        if (!line.empty() && line != "stack traceback:") {
            out += (out.empty() ? "" : " | ") + std::string{line};
        }
        start = end + 1;
    }
    return out;
}

/// The error itself, without the traceback under it.
std::string firstLine(const std::string& text) {
    return text.substr(0, text.find('\n'));
}

} // namespace

void Outbox::post(WorkerNews news) {
    const std::lock_guard lock{mutex_};
    news_.push_back(std::move(news));
}

std::vector<WorkerNews> Outbox::take() {
    const std::lock_guard lock{mutex_};
    return std::exchange(news_, {});
}

void PluginWorker::Inbox::post(std::function<void()> task) {
    {
        const std::lock_guard lock{mutex};
        if (closed) {
            return;
        }
        tasks.push_back(std::move(task));
        ++outstanding;
    }
    wake.notify_one();
}

void PluginWorker::Inbox::begin() {
    const std::lock_guard lock{mutex};
    ++outstanding;
}

void PluginWorker::Inbox::settle() {
    const std::lock_guard lock{mutex};
    --outstanding;
    if (outstanding == 0) {
        settled.notify_all();
    }
}

PluginWorker::PluginWorker(Setup setup, IoPool& pool, const net::WebClient& web, Outbox& outbox)
    : setup_{std::move(setup)}, pool_{pool}, web_{web}, outbox_{outbox},
      thread_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
    inbox_->post([this] {
        load();
    });
}

PluginWorker::~PluginWorker() {
    stopping_.store(true);
    {
        const std::lock_guard lock{inbox_->mutex};
        inbox_->closed = true;
    }
    thread_.request_stop();
    inbox_->wake.notify_all();
    thread_.join();
}

void PluginWorker::refresh(std::vector<library::Game> games) {
    inbox_->post([this, games = std::move(games)]() mutable {
        wanted_ = std::move(games);
        if (loaded_ && !refreshing_ && !halted_) {
            beginRefresh();
        }
    });
}

bool PluginWorker::waitIdle(std::chrono::milliseconds timeout) {
    std::unique_lock lock{inbox_->mutex};
    return inbox_->settled.wait_for(lock, timeout, [this] {
        return inbox_->outstanding == 0;
    });
}

void PluginWorker::run(const std::stop_token& stop) {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock{inbox_->mutex};
            if (!inbox_->wake.wait(lock, stop, [this] {
                    return !inbox_->tasks.empty();
                })) {
                return;
            }
            task = std::move(inbox_->tasks.front());
            inbox_->tasks.pop_front();
        }
        if (!halted_) {
            try {
                task();
            } catch (const std::exception& thrown) {
                halt(std::string{"the runtime failed: "} + thrown.what());
            }
        }
        inbox_->settle();
    }
}

void PluginWorker::load() {
    const std::string& id = setup_.manifest.id;
    try {
        runtime_ = std::make_unique<LuaRuntime>(setup_.limits, stopping_);
    } catch (const std::exception& thrown) {
        halt(thrown.what());
        return;
    }
    context_ = std::make_unique<HostContext>();
    context_->id = id;
    context_->channel = "plugin." + id;
    context_->permissions = setup_.permissions;
    context_->web = &web_;
    std::string error;
    if (!installHostApi(*runtime_, *context_, error)) {
        halt("cannot set up the opensu table: " + error);
        return;
    }
    std::ifstream in{setup_.dir / "main.lua", std::ios::binary};
    if (!in) {
        halt("main.lua is missing");
        return;
    }
    std::string source(sourceLimit + 1, '\0');
    in.read(source.data(), static_cast<std::streamsize>(source.size()));
    source.resize(static_cast<std::size_t>(in.gcount()));
    if (source.size() > sourceLimit) {
        halt("main.lua is over " + std::to_string(sourceLimit) + " bytes");
        return;
    }
    const int main = runtime_->compile(source, "main.lua", error);
    if (main == LUA_NOREF) {
        halt(error);
        return;
    }
    start(
        main,
        [](lua_State*) {
            return 0;
        },
        [this](Activation&) {
            context_->loading = false;
            if (!context_->playTime && !context_->gameList) {
                halt("main.lua registered no source");
                return;
            }
            loaded_ = true;
            outbox_.post(news(WorkerNews::Kind::Loaded, {}));
            if (wanted_) {
                beginRefresh();
            }
        },
        [this](const std::string& message) {
            halt(message);
        });
}

WorkerNews PluginWorker::news(WorkerNews::Kind kind, std::string message) const {
    WorkerNews made;
    made.plugin = setup_.manifest.id;
    made.kind = kind;
    made.message = std::move(message);
    return made;
}

std::vector<library::Game> PluginWorker::claimed(const Claims& claims) const {
    std::vector<library::Game> games;
    std::ranges::copy_if(games_, std::back_inserter(games), [&claims](const library::Game& game) {
        return claims.covers(game);
    });
    return games;
}

void PluginWorker::beginRefresh() {
    if (!wanted_) {
        return;
    }
    refreshing_ = true;
    games_ = std::move(*wanted_);
    wanted_.reset();
    listed_.clear();
    matches_.clear();
    hasList_ = context_->gameList.has_value();
    if (!hasList_) {
        runPlayTime();
        return;
    }
    start(
        context_->gameList->function,
        [](lua_State*) {
            return 0;
        },
        [this](Activation& finished) {
            std::string error;
            const bool read = finished.results(
                [this](lua_State* state, int count) {
                    if (count == 0) {
                        throw std::runtime_error{"returned nothing"};
                    }
                    listed_ = readGameList(state, -count);
                },
                error);
            if (!read) {
                endRefresh("list " + error, std::nullopt);
                return;
            }
            matches_ = library::matchGameList(claimed(context_->gameList->claims), listed_);
            runPlayTime();
        },
        [this](const std::string& message) {
            endRefresh(message, std::nullopt);
        });
}

void PluginWorker::runPlayTime() {
    if (!context_->playTime) {
        endRefresh({}, std::nullopt);
        return;
    }
    auto games =
        std::make_shared<const std::vector<library::Game>>(claimed(context_->playTime->claims));
    start(
        context_->playTime->function,
        [this, games](lua_State* state) {
            pushGames(state, *games, listed_, matches_);
            return 1;
        },
        [this, games](Activation& finished) {
            std::optional<library::PlayTimeReports::Report> report;
            std::string error;
            const bool read = finished.results(
                [&report, &games](lua_State* state, int count) {
                    if (count > 0) {
                        report = readPlayTime(state, -count, *games);
                    }
                },
                error);
            if (!read) {
                endRefresh("read " + error, std::nullopt);
                return;
            }
            endRefresh({}, std::move(report));
        },
        [this](const std::string& message) {
            endRefresh(message, std::nullopt);
        });
}

void PluginWorker::endRefresh(const std::string& failure,
                              std::optional<library::PlayTimeReports::Report> report) {
    refreshing_ = false;
    if (!failure.empty()) {
        fault(failure);
    } else {
        faults_ = 0;
        WorkerNews reported = news(WorkerNews::Kind::Report, {});
        reported.playTime = std::move(report);
        if (hasList_) {
            reported.listed = listed_.size();
        }
        reported.matched = matches_.size();
        outbox_.post(std::move(reported));
    }
    if (!halted_ && wanted_) {
        beginRefresh();
    }
}

void PluginWorker::start(int function, const std::function<int(lua_State*)>& arguments,
                         std::function<void(Activation&)> done,
                         std::function<void(const std::string&)> failed) {
    std::string error;
    std::unique_ptr<Activation> activation =
        Activation::start(*runtime_, function, arguments, error);
    if (!activation) {
        failed(error);
        return;
    }
    const std::uint64_t id = nextActivation_++;
    running_.emplace(id, Running{std::move(activation), std::move(done), std::move(failed)});
    step(id);
}

void PluginWorker::step(std::uint64_t id) {
    const auto found = running_.find(id);
    if (found == running_.end()) {
        return;
    }
    std::string error;
    switch (found->second.activation->resume(error)) {
    case Activation::Status::Waiting: {
        IoWork work = found->second.activation->takeWork();
        inbox_->begin();
        pool_.post([this, inbox = inbox_, work = std::move(work), id] {
            Pusher pusher;
            try {
                pusher = work();
            } catch (const std::exception& thrown) {
                pusher = [message = std::string{thrown.what()}](lua_State* state) {
                    lua_pushnil(state);
                    lua_pushlstring(state, message.data(), message.size());
                    return 2;
                };
            }
            // Runs only while the worker's thread does, which the worker joins before it goes.
            inbox->post([this, id, pusher = std::move(pusher)]() mutable {
                if (const auto waiting = running_.find(id); waiting != running_.end()) {
                    waiting->second.activation->deliver(std::move(pusher));
                    step(id);
                }
            });
            inbox->settle();
        });
        return;
    }
    case Activation::Status::Finished: {
        auto node = running_.extract(found);
        node.mapped().done(*node.mapped().activation);
        return;
    }
    case Activation::Status::Failed: {
        auto node = running_.extract(found);
        node.mapped().failed(error);
        return;
    }
    }
}

void PluginWorker::fault(const std::string& message) {
    ++faults_;
    if (faults_ >= faultLimit) {
        halt(message);
        return;
    }
    lucent::warn(context_->channel, "fault {} of {}: {}", faults_, faultLimit, oneLine(message));
    outbox_.post(news(WorkerNews::Kind::Fault, firstLine(message)));
}

void PluginWorker::halt(const std::string& message) {
    halted_ = true;
    lucent::error("plugin." + setup_.manifest.id, "stopped: {}", oneLine(message));
    outbox_.post(news(WorkerNews::Kind::Stopped, firstLine(message)));
}

} // namespace opensu::plugin
