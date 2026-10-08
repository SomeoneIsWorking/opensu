// Serves files by path and answers a Range request with 206, as a CDN does.
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>

#include "support/loopback_server.hpp"

namespace iideck::artwork::fixture {

class RangeServer {
  public:
    RangeServer()
        : server_{[this](const lucent::http::Request& request) {
              return answer(request);
          }} {
    }

    [[nodiscard]] std::string base() const {
        return server_.base();
    }

    /// Serves `body` at `path`.
    void put(const std::string& path, std::string body) {
        const std::lock_guard lock{mutex_};
        files_[path] = std::move(body);
    }
    /// Answers every request 200 with the whole body, as a server that ignores Range.
    void ignoreRange(bool ignore) {
        ignoreRange_ = ignore;
    }
    [[nodiscard]] std::uint64_t requests() const {
        return requests_;
    }
    [[nodiscard]] std::uint64_t bytesSent() const {
        return bytesSent_;
    }

  private:
    lucent::http::Response answer(const lucent::http::Request& request) {
        ++requests_;
        std::string body;
        {
            const std::lock_guard lock{mutex_};
            const auto file = files_.find(std::string{request.path()});
            if (file == files_.end()) {
                return test::reply(404, "");
            }
            body = file->second;
        }
        const auto range = request.header("Range");
        if (!range || ignoreRange_ || !range->starts_with("bytes=")) {
            bytesSent_ += body.size();
            return lucent::http::Response::binary(200, "OK", "application/octet-stream", body);
        }
        const std::string spec{range->substr(6)};
        const std::uint64_t first = std::strtoull(spec.c_str(), nullptr, 10);
        const std::uint64_t last = std::min<std::uint64_t>(
            std::strtoull(spec.c_str() + spec.find('-') + 1, nullptr, 10), body.size() - 1);
        lucent::http::Response response =
            lucent::http::Response::binary(206, "Partial Content", "application/octet-stream",
                                           body.substr(first, last - first + 1));
        response.headers.push_back({"Content-Range", "bytes " + std::to_string(first) + "-" +
                                                         std::to_string(last) + "/" +
                                                         std::to_string(body.size())});
        bytesSent_ += response.body.size();
        return response;
    }

    std::atomic<bool> ignoreRange_{false};
    std::atomic<std::uint64_t> requests_{0};
    std::atomic<std::uint64_t> bytesSent_{0};
    std::mutex mutex_;
    std::map<std::string, std::string> files_;
    test::LoopbackServer server_;
};

} // namespace iideck::artwork::fixture
