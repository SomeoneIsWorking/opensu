// The guarded request plugins fetch through: every redirect is asked about before it is followed,
// a body over the cap is refused, and any status comes back with its body. A download asks about
// its redirects the same way, and only the last hop's body is kept. A request with progress is
// told of its bytes and can be cancelled. A request or GET on a stop token ends as soon as the stop
// is requested, while it still waits for the answer.
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
    std::string asked;
    const net::WebClient::Guard open{.maxBody = 1024, .refusal = [&asked](const std::string& url) {
                                         asked += url + " ";
                                         return std::string{};
                                     }};
    std::string error;
    const auto followed = web.request(server.base() + "/hop", {}, open, error);
    expect(followed && followed->status == 200 && followed->body == "arrived",
           "an allowed redirect is followed");
    expect(asked == server.base() + "/hop " + server.base() + "/end ", "each hop is asked about");

    const net::WebClient::Guard closed{.maxBody = 1024, .refusal = [](const std::string& url) {
                                           return url.ends_with("/end") ? "no" : std::string{};
                                       }};
    expect(!web.request(server.base() + "/hop", {}, closed, error) && error == "no",
           "a refused redirect is not followed, with the refusal's reason");
    expect(!web.request(server.base() + "/big", {}, open, error) &&
               error == "the answer is over 1024 bytes",
           "a body over the cap is refused");
    const auto missing = web.request(server.base() + "/nothing", {}, open, error);
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
    const auto big = web.request(server.base() + "/big", {}, told, error);
    expect(big && big->body.size() == 4096 && seen == 4096 && announced == 4096,
           "a request with progress is told of its bytes and the announced size");
    const net::WebClient::Guard stopped{
        .maxBody = 8192, .refusal = open.refusal, .progress = [](std::uint64_t, std::uint64_t) {
            return false;
        }};
    expect(!web.request(server.base() + "/big", {}, stopped, error) && error == "cancelled",
           "a request its progress stops is cancelled");

    const auto stoppedAfter = [](std::stop_source& source) {
        return std::jthread{[&source] {
            std::this_thread::sleep_for(std::chrono::milliseconds{200});
            source.request_stop();
        }};
    };
    std::stop_source requestStop;
    const net::WebClient::Guard onStop{
        .maxBody = 1024, .refusal = open.refusal, .stop = requestStop.get_token()};
    auto begun = std::chrono::steady_clock::now();
    {
        const std::jthread stopper = stoppedAfter(requestStop);
        expect(!web.request(server.base() + "/slow", {}, onStop, error) && error == "cancelled",
               "a request whose stop is requested is cancelled");
    }
    expect(std::chrono::steady_clock::now() - begun < std::chrono::seconds{2},
           "a request ends as its stop is requested, not when the answer comes");
    std::stop_source getStop;
    begun = std::chrono::steady_clock::now();
    {
        const std::jthread stopper = stoppedAfter(getStop);
        expect(!web.get(server.base() + "/slow", error, {}, getStop.get_token()) &&
                   error == "cancelled",
               "a GET whose stop is requested is cancelled");
    }
    expect(std::chrono::steady_clock::now() - begun < std::chrono::seconds{2},
           "a GET ends as its stop is requested");
    std::stop_source unused;
    const net::WebClient::Guard notStopped{
        .maxBody = 8192, .refusal = open.refusal, .stop = unused.get_token()};
    const auto whole = web.request(server.base() + "/big", {}, notStopped, error);
    expect(whole && whole->body.size() == 4096, "a request on a stop never requested completes");

    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "web_client"};
    const std::filesystem::path file = scratch.path() / "body";
    const net::WebClient::Progress any = [](std::uint64_t, std::uint64_t) {
        return true;
    };
    asked.clear();
    expect(web.download(server.base() + "/hop", file, any, error, open.refusal),
           "a download follows an allowed redirect");
    std::ifstream in{file, std::ios::binary};
    expect(std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}} ==
                   "arrived" &&
               asked == server.base() + "/hop " + server.base() + "/end ",
           "asking about each hop and keeping only the last body");
    expect(!web.download(server.base() + "/hop", file, any, error, closed.refusal) && error == "no",
           "a download does not follow a refused redirect");
    expect(web.download(server.base() + "/hop", file, any, error),
           "without a refusal, a redirect within the scheme is followed");
    std::printf("web_client: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
