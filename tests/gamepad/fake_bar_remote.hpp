// A Wii Remote synced to a DolphinBar slot, as the slot's node shows it to openSU: it answers a
// status request with its status, an extension's register writes with acks and the extension id
// read with its id, keeps the report mode and player lights asked for, and sends the button and
// extension reports a test asks for in that mode. It stands on the far end of a socket pair or a
// pseudo-terminal, whose fd it does not own; requests are split by their sizes, as a terminal
// may join them.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <unistd.h>

namespace opensu::test {

class FakeBarRemote {
  public:
    enum class Extension : std::uint8_t {
        None,
        Nunchuk,
        Classic,
    };

    explicit FakeBarRemote(int fd) : fd_{fd} {
    }

    /// Plugs `extension` in (or pulls it out), sending the status that says so.
    void plug(Extension extension) {
        extension_ = extension;
        sendStatus();
    }

    /// Answers every request waiting; the number taken.
    int serve() {
        std::array<std::uint8_t, 256> buffer{};
        const ssize_t got = ::read(fd_, buffer.data(), buffer.size());
        if (got <= 0) {
            return 0;
        }
        pending_.insert(pending_.end(), buffer.begin(), buffer.begin() + got);
        int taken = 0;
        while (!pending_.empty()) {
            const std::size_t size = sizeOf(pending_.front());
            if (size == 0) {
                pending_.clear();
                break;
            }
            if (pending_.size() < size) {
                break;
            }
            const std::vector<std::uint8_t> request{pending_.begin(),
                                                    pending_.begin() + static_cast<long>(size)};
            pending_.erase(pending_.begin(), pending_.begin() + static_cast<long>(size));
            answer(request);
            ++taken;
        }
        return taken;
    }

    /// Sends the buttons (the two core bytes) and the extension's six bytes, in the mode asked.
    void report(std::uint8_t first, std::uint8_t second,
                std::array<std::uint8_t, 6> extension = idle()) {
        if (mode_ == 0x32) {
            std::array<std::uint8_t, 11> out{0x32, first, second};
            std::copy(extension.begin(), extension.end(), out.begin() + 3);
            send(out);
        } else {
            const std::array<std::uint8_t, 3> out{0x30, first, second};
            send(out);
        }
    }

    /// The extension's six bytes at rest: sticks centred, every button up (1 is up).
    [[nodiscard]] static std::array<std::uint8_t, 6> idle() {
        return {0x80, 0x80, 0x80, 0x80, 0xff, 0xff};
    }

    /// A Classic Controller's six bytes at rest: sticks centred, every button up.
    [[nodiscard]] static std::array<std::uint8_t, 6> classicIdle() {
        // Left stick 32, 32; right stick 16, 16; triggers out.
        return {0xa0, 0x20, 0x10, 0x00, 0xff, 0xff};
    }

    [[nodiscard]] std::uint8_t mode() const noexcept {
        return mode_;
    }
    [[nodiscard]] std::uint8_t lights() const noexcept {
        return lights_;
    }
    [[nodiscard]] bool unencrypted() const noexcept {
        return unlocked_ && zeroed_;
    }

  private:
    static std::size_t sizeOf(std::uint8_t id) {
        switch (id) {
        case 0x11:
        case 0x15:
            return 2;
        case 0x12:
            return 3;
        case 0x16:
            return 22;
        case 0x17:
            return 7;
        default:
            return 0;
        }
    }

    void answer(const std::vector<std::uint8_t>& request) {
        switch (request.front()) {
        case 0x11:
            lights_ = request[1];
            break;
        case 0x12:
            mode_ = request[2];
            break;
        case 0x15:
            sendStatus();
            break;
        case 0x16: {
            const std::uint32_t address =
                (std::uint32_t{request[2]} << 16) | (std::uint32_t{request[3]} << 8) | request[4];
            unlocked_ = unlocked_ || (address == 0xa400f0 && request[6] == 0x55);
            zeroed_ = zeroed_ || (address == 0xa400fb && request[6] == 0x00);
            const std::array<std::uint8_t, 5> ack{0x22, 0, 0, 0x16, 0};
            send(ack);
            break;
        }
        case 0x17: {
            std::array<std::uint8_t, 22> reply{0x21, 0, 0, 0x50, 0x00, 0xfa};
            const std::array<std::uint8_t, 6> id =
                extension_ == Extension::Classic
                    ? std::array<std::uint8_t, 6>{0, 0, 0xa4, 0x20, 1, 1}
                    : std::array<std::uint8_t, 6>{0, 0, 0xa4, 0x20, 0, 0};
            std::copy(id.begin(), id.end(), reply.begin() + 6);
            send(reply);
            break;
        }
        default:
            break;
        }
    }

    void sendStatus() {
        // A status ends the report mode until it is asked again.
        mode_ = 0;
        const std::array<std::uint8_t, 7> status{
            0x20, 0, 0,   static_cast<std::uint8_t>(extension_ != Extension::None ? 0x02 : 0),
            0,    0, 0xc0};
        send(status);
    }

    void send(std::span<const std::uint8_t> bytes) const {
        [[maybe_unused]] const ssize_t sent = ::write(fd_, bytes.data(), bytes.size());
    }

    int fd_;
    Extension extension_{Extension::None};
    std::uint8_t mode_{0};
    std::uint8_t lights_{0};
    bool unlocked_{false};
    bool zeroed_{false};
    std::vector<std::uint8_t> pending_;
};

} // namespace opensu::test
