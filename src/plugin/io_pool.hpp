// io_pool — the threads every plugin's file reads and HTTP requests run on, so a slow request
// stalls neither the frame nor that plugin's other callbacks. Jobs run in the order posted.
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace opensu::plugin {

class IoPool {
  public:
    static constexpr std::size_t defaultThreads = 2;

    explicit IoPool(std::size_t threads = defaultThreads);
    /// Runs no job not yet started; waits for the running ones.
    ~IoPool();
    IoPool(const IoPool&) = delete;
    IoPool& operator=(const IoPool&) = delete;
    IoPool(IoPool&&) = delete;
    IoPool& operator=(IoPool&&) = delete;

    /// Any thread.
    void post(std::function<void()> job);

  private:
    void run(const std::stop_token& stop);

    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<std::function<void()>> jobs_;
    /// Last, so the threads stop before the queue goes.
    std::vector<std::jthread> threads_;
};

} // namespace opensu::plugin
