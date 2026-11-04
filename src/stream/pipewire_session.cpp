#include "pipewire_session.hpp"

#include <stdexcept>
#include <string>

#include <spa/utils/result.h>

#include "lucent/log.h"

namespace opensu::stream {
namespace {

/// How long the daemon may take to answer.
constexpr int answerSeconds = 5;

} // namespace

PipeWireSession::PipeWireSession(const char* name) {
    pw_init(nullptr, nullptr);
    loop_ = pw_thread_loop_new(name, nullptr);
    if (loop_ == nullptr) {
        pw_deinit();
        throw std::runtime_error{"cannot start PipeWire's loop"};
    }
    try {
        if (pw_thread_loop_start(loop_) != 0) {
            throw std::runtime_error{"cannot start PipeWire's loop"};
        }
        const Lock lock{*this};
        context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
        if (context_ == nullptr) {
            throw std::runtime_error{"cannot make a PipeWire context"};
        }
        core_ = pw_context_connect(context_, nullptr, 0);
        if (core_ == nullptr) {
            throw std::runtime_error{"PipeWire is not running"};
        }
        static constexpr pw_core_events events{
            .version = PW_VERSION_CORE_EVENTS,
            .info = nullptr,
            .done = &PipeWireSession::onDone,
            .ping = nullptr,
            .error = &PipeWireSession::onError,
            .remove_id = nullptr,
            .bound_id = nullptr,
            .add_mem = nullptr,
            .remove_mem = nullptr,
            .bound_props = nullptr,
        };
        pw_core_add_listener(core_, &listener_, &events, this);
    } catch (...) {
        close();
        throw;
    }
}

PipeWireSession::~PipeWireSession() {
    close();
}

void PipeWireSession::settle(const Lock& /*lock*/) {
    settled_ = false;
    error_.clear();
    waitingFor_ = pw_core_sync(core_, PW_ID_CORE, 0);
    while (!settled_ && error_.empty()) {
        if (pw_thread_loop_timed_wait(loop_, answerSeconds) != 0) {
            throw std::runtime_error{"PipeWire did not answer"};
        }
    }
    if (!error_.empty()) {
        throw std::runtime_error{"PipeWire: " + error_};
    }
}

void PipeWireSession::close() noexcept {
    if (loop_ == nullptr) {
        return;
    }
    pw_thread_loop_stop(loop_);
    if (core_ != nullptr) {
        pw_core_disconnect(core_);
    }
    if (context_ != nullptr) {
        pw_context_destroy(context_);
    }
    pw_thread_loop_destroy(loop_);
    loop_ = nullptr;
    pw_deinit();
}

void PipeWireSession::onDone(void* data, std::uint32_t id, int seq) {
    auto& self = *static_cast<PipeWireSession*>(data);
    if (id == PW_ID_CORE && seq == self.waitingFor_) {
        self.settled_ = true;
        pw_thread_loop_signal(self.loop_, false);
    }
}

void PipeWireSession::onError(void* data, std::uint32_t id, int /*seq*/, int result,
                              const char* message) {
    auto& self = *static_cast<PipeWireSession*>(data);
    lucent::warn("pipewire", "error on {}: {} ({})", id, message, spa_strerror(result));
    self.error_ = message;
    pw_thread_loop_signal(self.loop_, false);
}

void PipeWireSession::connectInput(pw_stream* stream, std::span<const spa_pod*> params,
                                   const std::string& what) {
    // PipeWire's flags are a C bitmask enum: its API takes their OR through the enum type.
    // NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
    const auto flags =
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
    // NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)
    if (const int failed =
            pw_stream_connect(stream, PW_DIRECTION_INPUT, PW_ID_ANY, flags, params.data(),
                              static_cast<std::uint32_t>(params.size()));
        failed < 0) {
        throw std::runtime_error{what + ": " + spa_strerror(failed)};
    }
}

} // namespace opensu::stream
