// webdav_share — a WebDAV folder (Nextcloud, ownCloud, a NAS) as a `SaveShare`: GET to read, PUT
// beside the file then MOVE over it to replace, MKCOL for a missing folder, with Basic
// credentials over HTTPS (plain HTTP only to this machine).
#pragma once

#include <string>

#include "net/web_client.hpp"
#include "save_share.hpp"

namespace opensu::save_sync {

class WebDavShare final : public SaveShare {
  public:
    /// `url` is the folder's address, ending with `/` or not.
    WebDavShare(const net::WebClient& web, std::string url, std::string user, std::string password);
    ~WebDavShare() override;
    WebDavShare(const WebDavShare&) = delete;
    WebDavShare& operator=(const WebDavShare&) = delete;
    WebDavShare(WebDavShare&&) = delete;
    WebDavShare& operator=(WebDavShare&&) = delete;

    [[nodiscard]] std::string where() const override;
    [[nodiscard]] bool reachable() const override;
    [[nodiscard]] Read read(const std::string& name) const override;
    [[nodiscard]] bool write(const std::string& name, const std::string& bytes,
                             std::string& error) const override;

  private:
    /// `method` of `name` with `body`; nothing, with `error`, when it did not arrive.
    [[nodiscard]] std::optional<net::WebClient::Answer>
    send(const std::string& method, const std::string& name, std::optional<std::string> body,
         const std::vector<std::string>& headers, std::string& error) const;
    /// Makes each folder `name` lies in that is missing.
    [[nodiscard]] bool makeFolders(const std::string& name, std::string& error) const;
    [[nodiscard]] std::string urlOf(const std::string& name) const;

    const net::WebClient& web_;
    std::string url_;
    std::string user_;
    std::string password_;
};

} // namespace opensu::save_sync
