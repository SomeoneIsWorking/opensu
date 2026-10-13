#include "lua_runtime.hpp"

#include <array>
#include <cstdlib>
#include <utility>

#include "lauxlib.h"
#include "lualib.h"

namespace opensu::plugin {
namespace {

using Clock = std::chrono::steady_clock;

struct ProtectedCall {
    const std::function<void(lua_State*)>* work;
    std::string failure;
};

int runProtected(lua_State* state) {
    auto* call = static_cast<ProtectedCall*>(lua_touserdata(state, 1));
    lua_settop(state, 0);
    try {
        (*call->work)(state);
    } catch (const std::exception& thrown) {
        call->failure = thrown.what();
    }
    if (!call->failure.empty()) {
        return luaL_error(state, "%s", call->failure.c_str());
    }
    return 0;
}

/// The error object on top of `state` as text; never converts, so it cannot allocate.
std::string errorText(lua_State* state) {
    if (lua_type(state, -1) == LUA_TSTRING) {
        return lua_tostring(state, -1);
    }
    return "an error that is not a string";
}

} // namespace

LuaRuntime::LuaRuntime(Limits limits, const std::atomic<bool>& stopping)
    : limits_{limits}, stopping_{stopping} {
    state_ = lua_newstate(&LuaRuntime::allocate, this);
    if (state_ == nullptr) {
        throw std::runtime_error{"cannot create a Lua state"};
    }
    *static_cast<LuaRuntime**>(lua_getextraspace(state_)) = this;
    lua_atpanic(state_, &LuaRuntime::panic);
    lua_sethook(state_, &LuaRuntime::countHook, LUA_MASKCOUNT, hookInterval);
    openLibraries();
}

LuaRuntime::~LuaRuntime() {
    lua_close(state_);
}

LuaRuntime& LuaRuntime::of(lua_State* state) noexcept {
    return **static_cast<LuaRuntime**>(lua_getextraspace(state));
}

void LuaRuntime::openLibraries() {
    std::string error;
    const bool opened = protect(
        [](lua_State* state) {
            const std::array<luaL_Reg, 5> libraries{{{LUA_GNAME, luaopen_base},
                                                     {LUA_STRLIBNAME, luaopen_string},
                                                     {LUA_TABLIBNAME, luaopen_table},
                                                     {LUA_MATHLIBNAME, luaopen_math},
                                                     {LUA_UTF8LIBNAME, luaopen_utf8}}};
            for (const luaL_Reg& library : libraries) {
                luaL_requiref(state, library.name, library.func, 1);
                lua_pop(state, 1);
            }
            // Loading code, files and bytecode, steering the collector, and printing past the log.
            for (const char* removed :
                 {"dofile", "loadfile", "load", "collectgarbage", "print", "require"}) {
                lua_pushnil(state);
                lua_setglobal(state, removed);
            }
            lua_getglobal(state, LUA_STRLIBNAME);
            lua_pushnil(state);
            lua_setfield(state, -2, "dump");
            lua_pop(state, 1);
        },
        error);
    if (!opened) {
        throw std::runtime_error{"cannot open the Lua libraries: " + error};
    }
}

bool LuaRuntime::protect(const std::function<void(lua_State*)>& work, std::string& error) {
    const int top = lua_gettop(state_);
    ProtectedCall call{&work, {}};
    lua_pushcfunction(state_, runProtected);
    lua_pushlightuserdata(state_, &call);
    const int status = lua_pcall(state_, 1, 0, 0);
    if (status != LUA_OK) {
        error = errorText(state_);
    }
    lua_settop(state_, top);
    return status == LUA_OK;
}

int LuaRuntime::compile(std::string_view source, const std::string& name, std::string& error) {
    int reference = LUA_NOREF;
    std::string failure;
    const bool ran = protect(
        [&](lua_State* state) {
            const std::string chunk = "=" + name;
            if (luaL_loadbufferx(state, source.data(), source.size(), chunk.c_str(), "t") !=
                LUA_OK) {
                failure = errorText(state);
                return;
            }
            reference = luaL_ref(state, LUA_REGISTRYINDEX);
        },
        error);
    if (ran && !failure.empty()) {
        error = failure;
    }
    return ran && failure.empty() ? reference : LUA_NOREF;
}

int LuaRuntime::await(lua_State* state, IoWork work) {
    if (running_ == nullptr || running_->thread_ != state || !lua_isyieldable(state)) {
        return luaL_error(state, "this call can only wait in a plugin callback, outside "
                                 "metamethods and sort comparators");
    }
    running_->work_ = std::move(work);
    return lua_yieldk(state, 0, 0, &LuaRuntime::continueAfterAwait);
}

int LuaRuntime::continueAfterAwait(lua_State* state, int /*status*/, lua_KContext /*context*/) {
    Activation* running = of(state).running_;
    const Pusher pusher = std::exchange(running->delivered_, nullptr);
    if (!pusher) {
        return luaL_error(state, "resumed without a result");
    }
    return pusher(state);
}

void* LuaRuntime::allocate(void* self, void* block, std::size_t oldSize, std::size_t newSize) {
    auto& runtime = *static_cast<LuaRuntime*>(self);
    // With no block, oldSize is the kind of object, not a size.
    const std::size_t had = block != nullptr ? oldSize : 0;
    if (newSize == 0) {
        std::free(block);
        runtime.used_ -= had;
        return nullptr;
    }
    if (newSize > had && runtime.used_ - had + newSize > runtime.limits_.memoryBytes) {
        return nullptr;
    }
    void* moved = std::realloc(block, newSize);
    if (moved == nullptr) {
        // Lua assumes a shrink never fails: keep the larger block.
        return newSize <= had ? block : nullptr;
    }
    runtime.used_ = runtime.used_ - had + newSize;
    return moved;
}

void LuaRuntime::countHook(lua_State* state, lua_Debug* /*debug*/) {
    LuaRuntime& runtime = of(state);
    Activation* running = runtime.running_;
    if (running == nullptr) {
        luaL_error(state, "plugin code ran outside a callback");
        return;
    }
    if (runtime.stopping_.load()) {
        luaL_error(state, "stopped: the plugin is being closed");
        return;
    }
    running->instructions_ += hookInterval;
    if (running->instructions_ > runtime.limits_.instructions) {
        luaL_error(state, "ran over the limit of %I instructions",
                   static_cast<lua_Integer>(runtime.limits_.instructions));
        return;
    }
    if (running->ran_ + (Clock::now() - running->resumedAt_) > runtime.limits_.runTime) {
        luaL_error(state, "ran over the limit of %I ms",
                   static_cast<lua_Integer>(runtime.limits_.runTime.count()));
    }
}

int LuaRuntime::panic(lua_State* state) {
    throw LuaPanic{"Lua error outside a protected call: " + errorText(state)};
}

std::unique_ptr<Activation> Activation::start(LuaRuntime& runtime, int function,
                                              const std::function<int(lua_State*)>& pushArguments,
                                              std::string& error) {
    std::unique_ptr<Activation> made;
    const bool started = runtime.protect(
        [&](lua_State* state) {
            lua_State* thread = lua_newthread(state);
            const int reference = luaL_ref(state, LUA_REGISTRYINDEX);
            made.reset(new Activation{runtime, thread, reference});
            lua_rawgeti(state, LUA_REGISTRYINDEX, function);
            const int count = pushArguments(state);
            luaL_checkstack(thread, count + 1, "too many arguments");
            lua_xmove(state, thread, count + 1);
            made->arguments_ = count;
        },
        error);
    if (!started) {
        made.reset();
    }
    return made;
}

Activation::~Activation() {
    luaL_unref(runtime_.state_, LUA_REGISTRYINDEX, reference_);
}

Activation::Status Activation::resume(std::string& error) {
    const int arguments = started_ ? 0 : arguments_;
    started_ = true;
    runtime_.running_ = this;
    resumedAt_ = Clock::now();
    int results = 0;
    const int status = lua_resume(thread_, runtime_.state_, arguments, &results);
    ran_ += Clock::now() - resumedAt_;
    runtime_.running_ = nullptr;
    if (status == LUA_YIELD) {
        if (!work_) {
            error = "the plugin yielded outside a host call";
            return Status::Failed;
        }
        return Status::Waiting;
    }
    if (status == LUA_OK) {
        resultCount_ = results;
        return Status::Finished;
    }
    error = errorText(thread_);
    std::string traced;
    std::string ignored;
    if (runtime_.protect(
            [&](lua_State* state) {
                luaL_traceback(state, thread_, error.c_str(), 0);
                traced = lua_tostring(state, -1);
            },
            ignored)) {
        error = traced;
    }
    return Status::Failed;
}

bool Activation::results(const std::function<void(lua_State* state, int count)>& read,
                         std::string& error) {
    return runtime_.protect(
        [&](lua_State* state) {
            luaL_checkstack(state, resultCount_, "too many results");
            lua_xmove(thread_, state, resultCount_);
            read(state, resultCount_);
        },
        error);
}

} // namespace opensu::plugin
