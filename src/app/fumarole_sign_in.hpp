// fumarole_sign_in — signing fumarole in to the player's Steam account, on its own worker: with a
// QR code the Steam mobile app scans (`fumarole auth --qr <png>`, which rewrites the picture as
// Steam refreshes the code and deletes it once the sign-in is approved), or with the login this
// PC's Steam client saved (`fumarole auth --from-steam`). fumarole keeps the account; the loop
// takes how it ended with `take`.
#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace opensu::app {

class FumaroleSignIn {
  public:
    enum class Way : std::uint8_t {
        Qr,
        /// The login this PC's Steam client saved.
        FromSteam,
    };

    /// How a sign-in ended, with words for the player.
    struct Result {
        bool ok{false};
        std::string message;
    };

    /// `program` is fumarole, empty when it is not installed; `qr` where its QR picture goes.
    FumaroleSignIn(std::filesystem::path program, std::filesystem::path qr)
        : program_{std::move(program)}, qr_{std::move(qr)} {
    }

    /// Starts signing in `way`, ending one under way.
    void start(Way way);
    /// Ends a sign-in under way; it reports nothing.
    void cancel();
    /// The QR picture, there while a QR sign-in waits for the phone.
    [[nodiscard]] const std::filesystem::path& qr() const noexcept {
        return qr_;
    }
    /// How the last sign-in ended, once.
    [[nodiscard]] std::optional<Result> take();

  private:
    void run(const std::stop_token& stop, Way way);

    std::filesystem::path program_;
    std::filesystem::path qr_;
    std::mutex mutex_;
    std::optional<Result> result_;
    std::jthread worker_;
};

} // namespace opensu::app
