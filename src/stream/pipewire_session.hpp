// pipewire_session — one connection to the PipeWire daemon on a thread loop of its own, which the
// screen capture and the game's sound share the shape of. Linux only.
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include <pipewire/pipewire.h>

namespace opensu::stream {

class PipeWireSession {
  public:
    /// Starts a loop named `name` and connects; throws `std::runtime_error` saying why it cannot.
    explicit PipeWireSession(const char* name);
    ~PipeWireSession();
    PipeWireSession(const PipeWireSession&) = delete;
    PipeWireSession& operator=(const PipeWireSession&) = delete;
    PipeWireSession(PipeWireSession&&) = delete;
    PipeWireSession& operator=(PipeWireSession&&) = delete;

    [[nodiscard]] pw_thread_loop* loop() const noexcept {
        return loop_;
    }
    [[nodiscard]] pw_core* core() const noexcept {
        return core_;
    }

    /// Holds the loop's lock for a scope: what touches PipeWire objects off its thread takes it.
    class Lock {
      public:
        explicit Lock(const PipeWireSession& session) : loop_{session.loop_} {
            pw_thread_loop_lock(loop_);
        }
        ~Lock() {
            pw_thread_loop_unlock(loop_);
        }
        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
        Lock(Lock&&) = delete;
        Lock& operator=(Lock&&) = delete;

      private:
        pw_thread_loop* loop_;
    };

    /// Waits, holding the lock, until the daemon has handled everything asked of it so far; throws
    /// `std::runtime_error` when it does not answer or reports an error.
    void settle(const Lock& lock);

    /// Connects `stream` to take buffers in, mapped, from the node its properties target, offering
    /// `params`; throws `std::runtime_error` saying why it cannot, as `what`.
    static void connectInput(pw_stream* stream, std::span<const spa_pod*> params,
                             const std::string& what);

  private:
    void close() noexcept;
    static void onDone(void* data, std::uint32_t id, int seq);
    static void onError(void* data, std::uint32_t id, int seq, int result, const char* message);

    pw_thread_loop* loop_{};
    pw_context* context_{};
    pw_core* core_{};
    spa_hook listener_{};
    int waitingFor_{-1};
    bool settled_{false};
    std::string error_;
};

} // namespace opensu::stream
