// lua_runtime — one plugin's sandboxed Lua state: the allocator that caps its memory, the count
// hook that caps each callback's instructions and running time, the libraries it may use (base
// without load, dofile, loadfile, collectgarbage and print; string without dump; table, math,
// utf8), and protected entry. Each callback runs as an `Activation`, a coroutine that can wait on
// I/O done off the plugin's thread. Lua is compiled as C++, so its errors unwind as exceptions.
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "lua.h"

namespace opensu::plugin {

struct Limits {
    std::size_t memoryBytes{std::size_t{32} << 20U};
    /// Per callback, counted while it runs, not while it waits on I/O.
    std::uint64_t instructions{50'000'000};
    std::chrono::milliseconds runTime{5000};
};

/// How many instructions run between two checks of the limits.
inline constexpr int hookInterval = 10'000;

/// Pushes what a yielding call returns and says how many values.
using Pusher = std::function<int(lua_State*)>;
/// The work a yielding call has done off the plugin's thread; it hands back the call's results.
using IoWork = std::function<Pusher()>;

/// Lua could not raise an error at all, so the state cannot be used again.
class LuaPanic : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class Activation;

class LuaRuntime {
  public:
    /// `stopping` makes a running callback fail at its next check once set, from any thread.
    LuaRuntime(Limits limits, const std::atomic<bool>& stopping);
    ~LuaRuntime();
    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;
    LuaRuntime(LuaRuntime&&) = delete;
    LuaRuntime& operator=(LuaRuntime&&) = delete;

    /// The runtime that owns `state`, any thread of it.
    [[nodiscard]] static LuaRuntime& of(lua_State* state) noexcept;

    /// Runs `work` on the main state in a protected call. False with `error` when it raised.
    bool protect(const std::function<void(lua_State*)>& work, std::string& error);

    /// Compiles `source` as text (never bytecode) and keeps it in the registry: its reference, or
    /// `LUA_NOREF` with `error`.
    [[nodiscard]] int compile(std::string_view source, const std::string& name, std::string& error);

    /// From a binding: suspends the running callback until `work` has run off this thread, then
    /// returns what its pusher pushes. Raises when not in a callback or not where it may wait.
    int await(lua_State* state, IoWork work);

    [[nodiscard]] std::size_t memoryUsed() const noexcept {
        return used_;
    }
    [[nodiscard]] const Limits& limits() const noexcept {
        return limits_;
    }

  private:
    friend class Activation;

    static void* allocate(void* self, void* block, std::size_t oldSize, std::size_t newSize);
    static void countHook(lua_State* state, lua_Debug* debug);
    static int panic(lua_State* state);
    static int continueAfterAwait(lua_State* state, int status, lua_KContext context);
    void openLibraries();

    Limits limits_;
    std::size_t used_{0};
    const std::atomic<bool>& stopping_;
    Activation* running_{nullptr};
    lua_State* state_{nullptr};
};

/// One run of a callback, as a coroutine anchored in the registry.
class Activation {
  public:
    enum class Status : std::uint8_t {
        Finished,
        /// Waiting on the work `takeWork` hands over.
        Waiting,
        Failed,
    };

    /// A coroutine that calls the function with reference `function` on what `pushArguments`
    /// pushes (it returns the count); nothing with `error` when it cannot be made.
    [[nodiscard]] static std::unique_ptr<Activation>
    start(LuaRuntime& runtime, int function, const std::function<int(lua_State*)>& pushArguments,
          std::string& error);
    ~Activation();
    Activation(const Activation&) = delete;
    Activation& operator=(const Activation&) = delete;
    Activation(Activation&&) = delete;
    Activation& operator=(Activation&&) = delete;

    /// Runs until the callback finishes, waits or fails (`error` with a traceback). After
    /// `Waiting`, `deliver` the work's result before resuming.
    Status resume(std::string& error);

    [[nodiscard]] IoWork takeWork() {
        return std::exchange(work_, nullptr);
    }
    void deliver(Pusher pusher) {
        delivered_ = std::move(pusher);
    }

    /// Reads what the finished callback returned, in a protected call: `read` gets the main
    /// state with the results at the top, `count` of them.
    bool results(const std::function<void(lua_State* state, int count)>& read, std::string& error);

  private:
    friend class LuaRuntime;
    Activation(LuaRuntime& runtime, lua_State* thread, int reference)
        : runtime_{runtime}, thread_{thread}, reference_{reference} {
    }

    LuaRuntime& runtime_;
    lua_State* thread_;
    int reference_;
    int arguments_{0};
    int resultCount_{0};
    bool started_{false};
    std::uint64_t instructions_{0};
    std::chrono::steady_clock::duration ran_{};
    std::chrono::steady_clock::time_point resumedAt_{};
    IoWork work_;
    Pusher delivered_;
};

} // namespace opensu::plugin
