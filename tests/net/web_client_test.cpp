// The guarded request plugins fetch through: every redirect is asked about before it is followed,
// a body over the cap is refused, and any status comes back with its body. A download asks about
// its redirects the same way, and only the last hop's body is kept. A request with progress is
// told of its bytes and can be cancelled. Every kind of request ends Stopped as soon as its stop is
// requested, while it still waits for the answer, and one on a stop already requested is not made.
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stop_token>
#include <string>
#include <thread>

#include "net/web_client.hpp"
#include "support/loopback_server.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

} // namespace

int main() try {
    test::LoopbackServer* self = nullptr;
    test::LoopbackServer server{[&self](const lucent::http::Request& request) {
        const std::string path{request.path()};
        if (path == "/hop") {
            lucent::http::Response moved = test::reply(302, "");
            moved.headers.push_back({"Location", self->base() + "/end"});
            return moved;
        }
        if (path == "/end") {
            return test::reply(200, "arrived");
        }
        if (path == "/big") {
            return test::reply(200, std::string(4096, 'x'));
        }
        if (path == "/slow") {
            std::this_thread::sleep_for(std::chrono::seconds{5});
            return test::reply(200, "late");
        }
        return test::reply(404, "none");
    }};
    self = &server;
    const net::WebClient web;
    const std::stop_token never;
    std::string asked;
    const net::WebClient::Guard open{.maxBody = 1024, .refusal = [&asked](const std::string& url) {
                                         asked += url + " ";
                                         return std::string{};
                                     }};
    const auto followed = web.request(never, server.base() + "/hop", {}, open);
    expect(followed && followed->status == 200 && followed->body == "arrived",
           "an allowed redirect is followed");
    expect(asked == server.base() + "/hop " + server.base() + "/end ", "each hop is asked about");

    const net::WebClient::Guard closed{.maxBody = 1024, .refusal = [](const std::string& url) {
                                           return url.ends_with("/end") ? "no" : std::string{};
                                       }};
    const auto refused = web.request(never, server.base() + "/hop", {}, closed);
    expect(!refused && !refused.stopped() && refused.error() == "no",
           "a refused redirect is not followed, with the refusal's reason");
    expect(web.request(never, server.base() + "/big", {}, open).error() ==
               "the answer is over 1024 bytes",
           "a body over the cap is refused");
    const auto missing = web.request(never, server.base() + "/nothing", {}, open);
    expect(missing && missing->status == 404 && missing->body == "none",
           "another status comes back with its body");

    std::uint64_t seen = 0;
    std::uint64_t announced = 0;
    const net::WebClient::Guard told{.maxBody = 8192,
                                     .refusal = open.refusal,
                                     .progress = [&](std::uint64_t received, std::uint64_t total) {
                                         seen = received;
                                         announced = total;
                                         return true;
                                     }};
    const auto big = web.request(never, server.base() + "/big", {}, told);
    expect(big && big->body.size() == 4096 && seen == 4096 && announced == 4096,
           "a request with progress is told of its bytes and the announced size");
    const net::WebClient::Guard cancelling{
        .maxBody = 8192, .refusal = open.refusal, .progress = [](std::uint64_t, std::uint64_t) {
            return false;
        }};
    const auto cancelled = web.request(never, server.base() + "/big", {}, cancelling);
    expect(!cancelled && cancelled.stopped() && cancelled.error().empty(),
           "a request its progress cancels is stopped, with no error");

    const auto stoppedAfter = [](std::stop_source& source) {
        return std::jthread{[&source] {
            std::this_thread::sleep_for(std::chrono::milliseconds{200});
            source.request_stop();
        }};
    };
    const auto endsOnStop = [&](const std::string& what, const auto& ask) {
        std::stop_source source;
        const auto begun = std::chrono::steady_clock::now();
        bool stopped = false;
        {
            const std::jthread stopper = stoppedAfter(source);
            stopped = ask(source.get_token());
        }
        expect(stopped, (what + " whose stop is requested is stopped").c_str());
        expect(std::chrono::steady_clock::now() - begun < std::chrono::seconds{2},
               (what + " ends as its stop is requested, not when the answer comes").c_str());
    };
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "web_client"};
    const std::filesystem::path file = scratch.path() / "body";
    const net::WebClient::Progress any = [](std::uint64_t, std::uint64_t) {
        return true;
    };
    endsOnStop("a request", [&](const std::stop_token& stop) {
        return web.request(stop, server.base() + "/slow", {}, open).stopped();
    });
    endsOnStop("a GET", [&](const std::stop_token& stop) {
        return web.get(stop, server.base() + "/slow").stopped();
    });
    endsOnStop("a HEAD", [&](const std::stop_token& stop) {
        return web.head(stop, server.base() + "/slow", open.refusal).stopped();
    });
    endsOnStop("a download", [&](const std::stop_token& stop) {
        return web.download(stop, server.base() + "/slow", file, any).stopped();
    });
    std::stop_source already;
    already.request_stop();
    expect(web.get(already.get_token(), server.base() + "/end").stopped(),
           "a request on a stop already requested is not made");
    std::stop_source unused;
    const auto whole = web.request(unused.get_token(), server.base() + "/big", {}, told);
    expect(whole && whole->body.size() == 4096, "a request on a stop never requested completes");

    asked.clear();
    expect(web.download(never, server.base() + "/hop", file, any, open.refusal).ok(),
           "a download follows an allowed redirect");
    std::ifstream in{file, std::ios::binary};
    expect(std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}} ==
                   "arrived" &&
               asked == server.base() + "/hop " + server.base() + "/end ",
           "asking about each hop and keeping only the last body");
    expect(web.download(never, server.base() + "/hop", file, any, closed.refusal).error() == "no",
           "a download does not follow a refused redirect");
    expect(web.download(never, server.base() + "/hop", file, any).ok(),
           "without a refusal, a redirect within the scheme is followed");
    std::printf("web_client: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
