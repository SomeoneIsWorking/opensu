// steamgriddb_openid — SteamGridDB's "Sign in through Steam" as plain HTTP with the account's Steam
// web login (`steamLoginSecure`, from fumarole), then its API key: no Steam client, no browser.
#pragma once

#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "net/web_client.hpp"

namespace opensu::steam {

/// Names and values, in the order they came.
using Pairs = std::vector<std::pair<std::string, std::string>>;

/// The fields of Steam's OpenID sign-in form (`id="openidForm"`) in `page`; empty without one.
[[nodiscard]] Pairs openIdFormFields(std::string_view page);

/// The cookies the `Set-Cookie` lines of `headers` set, as name and value.
[[nodiscard]] Pairs setCookies(const Pairs& headers);

/// `fields` as a multipart/form-data body between `boundary` lines.
[[nodiscard]] std::string multipartBody(const Pairs& fields, std::string_view boundary);

/// The `api_key` in SteamGridDB's `/api/public/prefs` answer: empty when the account has none,
/// nothing when `text` is not a signed-in account's preferences.
[[nodiscard]] std::optional<std::string> prefsApiKey(std::string_view text);

/// Signs in to SteamGridDB through Steam with `login` and reads the account's API key, making one
/// when it has none. Nothing, with `error` saying why ("cancelled" when `stop` asked), otherwise.
[[nodiscard]] std::optional<std::string> steamGridDbKeyThroughOpenId(const net::WebClient& web,
                                                                     const std::stop_token& stop,
                                                                     const std::string& login,
                                                                     std::string& error);

} // namespace opensu::steam
