#include "io_pool.hpp"

#include <utility>

namespace opensu::plugin {

IoPool::IoPool(std::size_t threads) {
    for (std::size_t index = 0; index < threads; ++index) {
        threads_.emplace_back([this](const std::stop_token& stop) {
            run(stop);
        });
    }
}

IoPool::~IoPool() {
    for (std::jthread& thread : threads_) {
        thread.request_stop();
    }
    wake_.notify_all();
}

void IoPool::post(std::function<void()> job) {
    {
        const std::lock_guard lock{mutex_};
        jobs_.push_back(std::move(job));
    }
    wake_.notify_one();
}

void IoPool::run(const std::stop_token& stop) {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock lock{mutex_};
            if (!wake_.wait(lock, stop, [this] {
                    return !jobs_.empty();
                })) {
                return;
            }
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        job();
    }
}

} // namespace opensu::plugin
