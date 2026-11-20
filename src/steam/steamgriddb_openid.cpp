#include "steamgriddb_openid.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>

#include <nlohmann/json.hpp>

#include "lucent/log.h"
#include "steamgriddb_sign_in.hpp"

namespace opensu::steam {
namespace {

constexpr std::string_view openIdLogin = "https://steamcommunity.com/openid/login";
constexpr std::string_view openIdSpec = "http://specs.openid.net/auth/2.0";
constexpr std::string_view returnPath = "/login/steam";
constexpr std::string_view prefsPath = "/api/public/prefs";
constexpr std::string_view apiKeyPath = "/api/v2/users/apikey";
constexpr std::string_view boundary = "opensu-openid-form";
constexpr std::size_t maxPage = std::size_t{2} * 1024 * 1024;

/// The entities a form's attribute values carry, and their characters.
constexpr std::array<std::pair<std::string_view, char>, 6> entities{{{"&amp;", '&'},
                                                                     {"&quot;", '"'},
                                                                     {"&#39;", '\''},
                                                                     {"&#039;", '\''},
                                                                     {"&lt;", '<'},
                                                                     {"&gt;", '>'}}};

/// `text` with its entities turned back into characters.
std::string unescaped(std::string_view text) {
    std::string out;
    for (std::size_t at = 0; at < text.size();) {
        const auto entity = std::ranges::find_if(entities, [&](const auto& pair) {
            return text.substr(at).starts_with(pair.first);
        });
        if (entity != entities.end()) {
            out += entity->second;
            at += entity->first.size();
        } else {
            out += text[at];
            ++at;
        }
    }
    return out;
}

/// The value of attribute `name` in tag `tag`, quoted with `"`; nothing when it has none.
std::optional<std::string> attribute(std::string_view tag, std::string_view name) {
    const std::string marker = " " + std::string{name} + "=\"";
    const std::size_t at = tag.find(marker);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t from = at + marker.size();
    const std::size_t to = tag.find('"', from);
    if (to == std::string_view::npos) {
        return std::nullopt;
    }
    return unescaped(tag.substr(from, to - from));
}

/// `cookies` as a Cookie header.
std::string cookieHeader(const Pairs& cookies) {
    std::string header = "Cookie: ";
    for (std::size_t at = 0; at < cookies.size(); ++at) {
        header += (at == 0 ? "" : "; ") + cookies[at].first + "=" + cookies[at].second;
    }
    return header;
}

/// `fresh` set over `into`, a name set again keeping its new value.
void setOver(Pairs& into, const Pairs& fresh) {
    for (const auto& [name, value] : fresh) {
        const auto known = std::ranges::find(into, name, &std::pair<std::string, std::string>::first);
        if (known == into.end()) {
            into.emplace_back(name, value);
        } else {
            known->second = value;
        }
    }
}

std::string query(const Pairs& fields) {
    std::string out;
    for (const auto& [name, value] : fields) {
        out += (out.empty() ? "" : "&") + name + "=" + net::percentEncode(value);
    }
    return out;
}

/// Only Steam's sign-in and SteamGridDB, over https.
std::string refusal(const std::string& url) {
    const std::string origin = net::originOf(url);
    return origin == "https://steamcommunity.com/" ||
                   origin == std::string{steamGridDbOrigin} + "/"
               ? std::string{}
               : "the sign-in went to " + origin;
}

/// One step's failure as `error`, "cancelled" for a stop.
template <typename T>
std::nullopt_t failed(const net::Result<T>& result, std::string_view step, std::string& error) {
    error = result.stopped() ? "cancelled" : std::string{step} + ": " + result.error();
    return std::nullopt;
}

} // namespace

Pairs openIdFormFields(std::string_view page) {
    const std::size_t form = page.find("id=\"openidForm\"");
    if (form == std::string_view::npos) {
        return {};
    }
    const std::size_t end = page.find("</form>", form);
    const std::string_view body = page.substr(form, end == std::string_view::npos ? end : end - form);
    Pairs fields;
    for (std::size_t at = body.find("<input"); at != std::string_view::npos;
         at = body.find("<input", at + 1)) {
        const std::size_t close = body.find('>', at);
        if (close == std::string_view::npos) {
            break;
        }
        const std::string_view tag = body.substr(at, close - at);
        const std::optional<std::string> name = attribute(tag, "name");
        if (name && !name->empty()) {
            fields.emplace_back(*name, attribute(tag, "value").value_or(std::string{}));
        }
    }
    return fields;
}

Pairs setCookies(const Pairs& headers) {
    Pairs cookies;
    for (const auto& [name, value] : headers) {
        if (!std::ranges::equal(name, std::string_view{"set-cookie"}, [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == b;
            })) {
            continue;
        }
        const std::string_view pair = std::string_view{value}.substr(0, value.find(';'));
        const std::size_t equals = pair.find('=');
        if (equals != std::string_view::npos && equals > 0) {
            cookies.emplace_back(std::string{pair.substr(0, equals)},
                                 std::string{pair.substr(equals + 1)});
        }
    }
    return cookies;
}

std::string multipartBody(const Pairs& fields, std::string_view boundary) {
    std::string body;
    for (const auto& [name, value] : fields) {
        body += "--";
        body += boundary;
        body += "\r\nContent-Disposition: form-data; name=\"";
        body += name;
        body += "\"\r\n\r\n";
        body += value;
        body += "\r\n";
    }
    body += "--";
    body += boundary;
    body += "--\r\n";
    return body;
}

std::optional<std::string> prefsApiKey(std::string_view text) {
    const nlohmann::json prefs = nlohmann::json::parse(text, nullptr, false);
    if (!prefs.is_object()) {
        return std::nullopt;
    }
    const nlohmann::json* user = nullptr;
    if (const auto data = prefs.find("data"); data != prefs.end() && data->is_object()) {
        if (const auto found = data->find("user"); found != data->end() && found->is_object()) {
            user = &*found;
        }
    }
    if (user == nullptr) {
        return std::nullopt;
    }
    const auto key = user->find("api_key");
    return key != user->end() && key->is_string() ? key->get<std::string>() : std::string{};
}

std::optional<std::string> steamGridDbKeyThroughOpenId(const net::WebClient& web,
                                                       const std::stop_token& stop,
                                                       const std::string& login,
                                                       std::string& error) {
    const std::string origin{steamGridDbOrigin};
    const net::WebClient::Guard guard{.maxBody = maxPage, .refusal = refusal};
    Pairs steam{{"steamLoginSecure", login}};
    const std::string spec{openIdSpec};
    const std::string select = spec + "/identifier_select";
    const std::string ask = std::string{openIdLogin} + "?" +
                            query({{"openid.ns", spec},
                                   {"openid.mode", "checkid_setup"},
                                   {"openid.identity", select},
                                   {"openid.claimed_id", select},
                                   {"openid.realm", origin},
                                   {"openid.return_to", origin + std::string{returnPath}}});
    const net::Result<net::WebClient::Answer> page =
        web.request(stop, ask, {cookieHeader(steam)}, guard);
    if (!page) {
        return failed(page, "Steam's sign-in page", error);
    }
    const Pairs fields = openIdFormFields(page->body);
    if (page->status != 200 || fields.empty()) {
        error = "Steam did not take fumarole's web login (HTTP " + std::to_string(page->status) + ")";
        return std::nullopt;
    }
    setOver(steam, setCookies(page->headers));

    const net::Result<net::WebClient::Answer> confirmed = web.request(
        stop, std::string{openIdLogin},
        {cookieHeader(steam), "Content-Type: multipart/form-data; boundary=" + std::string{boundary}},
        guard, {.method = "POST", .body = multipartBody(fields, boundary), .follow = false});
    if (!confirmed) {
        return failed(confirmed, "Steam's sign-in", error);
    }
    const std::string back = confirmed->header("Location");
    if (!back.starts_with(origin + std::string{returnPath} + "?")) {
        error = "Steam's sign-in did not lead back to SteamGridDB (HTTP " +
                std::to_string(confirmed->status) + ")";
        return std::nullopt;
    }

    const net::Result<net::WebClient::Answer> signedIn =
        web.request(stop, back, {}, guard, {.follow = false});
    if (!signedIn) {
        return failed(signedIn, "SteamGridDB's sign-in", error);
    }
    Pairs gridDb = setCookies(signedIn->headers);
    if (gridDb.empty()) {
        error = "SteamGridDB kept no session (HTTP " + std::to_string(signedIn->status) + ")";
        return std::nullopt;
    }

    const net::Result<net::WebClient::Answer> prefs =
        web.request(stop, origin + std::string{prefsPath}, {cookieHeader(gridDb)}, guard);
    if (!prefs) {
        return failed(prefs, "SteamGridDB's preferences", error);
    }
    std::optional<std::string> key = prefsApiKey(prefs->body);
    if (!key) {
        error = "SteamGridDB did not sign in (HTTP " + std::to_string(prefs->status) + ")";
        return std::nullopt;
    }
    if (isSteamGridDbKey(*key)) {
        return key;
    }

    // No key yet: made as its API page's Create button does, with its XSRF token.
    setOver(gridDb, setCookies(prefs->headers));
    std::vector<std::string> headers{cookieHeader(gridDb)};
    const auto xsrf =
        std::ranges::find(gridDb, "XSRF-TOKEN", &std::pair<std::string, std::string>::first);
    if (xsrf != gridDb.end()) {
        headers.push_back("X-XSRF-TOKEN: " + net::percentDecode(xsrf->second));
    }
    const net::Result<net::WebClient::Answer> made = web.request(
        stop, origin + std::string{apiKeyPath}, headers, guard, {.method = "POST", .body = ""});
    if (!made) {
        return failed(made, "SteamGridDB's new key", error);
    }
    const nlohmann::json answer = nlohmann::json::parse(made->body, nullptr, false);
    const auto data = answer.is_object() ? answer.find("data") : answer.end();
    if (!answer.is_object() || data == answer.end() || !data->is_string() ||
        !isSteamGridDbKey(data->get_ref<const std::string&>())) {
        error = "SteamGridDB made no key (HTTP " + std::to_string(made->status) + ")";
        return std::nullopt;
    }
    lucent::info("steamgriddb", "made the account's API key");
    return data->get<std::string>();
}

} // namespace opensu::steam
