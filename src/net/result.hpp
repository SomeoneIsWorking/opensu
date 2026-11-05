// result — what a request comes to: its value, why it failed, or that its stop ended it. A stopped
// request is no error: whoever asked keeps nothing of it and logs nothing.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace opensu::net {

/// A request that failed, and why ("HTTP 404", a refusal, libcurl's reason).
struct Failure {
    std::string reason;
};

/// A request its stop ended, or its progress callback cancelled.
struct Stopped {};

template <typename T> class Result {
  public:
    // Implicit, so a function returns its value, a Failure or Stopped{} as it is.
    Result(T value) : state_{std::in_place_index<0>, std::move(value)} {
    }
    Result(Failure failure) : state_{std::move(failure)} {
    }
    Result(Stopped stopped) : state_{stopped} {
    }

    [[nodiscard]] bool ok() const noexcept {
        return state_.index() == 0;
    }
    explicit operator bool() const noexcept {
        return ok();
    }
    [[nodiscard]] bool stopped() const noexcept {
        return std::holds_alternative<Stopped>(state_);
    }
    /// Why it failed; empty when it holds a value or was stopped.
    [[nodiscard]] std::string error() const {
        const Failure* failure = std::get_if<Failure>(&state_);
        return failure != nullptr ? failure->reason : std::string{};
    }

    [[nodiscard]] T& operator*() & {
        return std::get<0>(state_);
    }
    [[nodiscard]] const T& operator*() const& {
        return std::get<0>(state_);
    }
    [[nodiscard]] T&& operator*() && {
        return std::get<0>(std::move(state_));
    }
    [[nodiscard]] T* operator->() {
        return &std::get<0>(state_);
    }
    [[nodiscard]] const T* operator->() const {
        return &std::get<0>(state_);
    }

    /// This failure or stop as a result of another type; only for one that holds no value.
    template <typename U> [[nodiscard]] Result<U> as() const {
        return as<U>({});
    }

    /// This failure with `context` before its reason, or this stop, as a result of another type.
    template <typename U> [[nodiscard]] Result<U> as(std::string_view context) const {
        if (stopped()) {
            return Stopped{};
        }
        return Failure{std::string{context} + error()};
    }

  private:
    std::variant<T, Failure, Stopped> state_;
};

} // namespace opensu::net
