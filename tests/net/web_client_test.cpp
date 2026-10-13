// The guarded request plugins fetch through: every redirect is asked about before it is followed,
// a body over the cap is refused, and any status comes back with its body.
#include <cstdio>
#include <string>

#include "net/web_client.hpp"
#include "support/loopback_server.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

} // namespace

int main() {
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
    std::printf("web_client: all checks passed\n");
    return 0;
}
