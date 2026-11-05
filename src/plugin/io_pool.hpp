// io_pool — the threads every plugin's file reads and HTTP requests run on, so a slow request
// stalls neither the frame nor that plugin's other callbacks. Jobs run in the order posted, each
// on its thread's stop token, which the pool's end requests.
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

    /// A job, given its thread's stop: requested when the pool goes.
    using Job = std::function<void(const std::stop_token&)>;

    /// Any thread.
    void post(Job job);

  private:
    void run(const std::stop_token& stop);

    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<Job> jobs_;
    /// Last, so the threads stop before the queue goes.
    std::vector<std::jthread> threads_;
};

} // namespace opensu::plugin
